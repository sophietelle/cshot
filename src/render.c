#include "render.h"
#include "cairo.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Frame callback */
static void frame_done(void *data, struct wl_callback *callback, uint32_t) {
    struct output_state *os = data;
    wl_callback_destroy(callback);
    os->frame_callback = nullptr;
    if (os->dirty) {
        os->dirty = false;
        render_output(os);
    }
}

static const struct wl_callback_listener frame_listener = {
    .done = frame_done
};

void render_output(struct output_state *os) {
    if (!os || !os->app || !os->wl_surface || !os->cairo_bg || !os->configured) {
        return;
    }

    if (os->frame_callback) {
        os->dirty = true;
        return;
    }

    struct render_buffer *rb = &os->buffers[0];
    for (int i = 1; i < RENDER_BUFFERS_COUNT; i++) {
        if (!os->buffers[i].busy) {
            rb = &os->buffers[i];
            break;
        }
    }

    rb->busy = true;

    // Быстро копируем фон (скриншот) в буфер
    size_t size = (size_t)os->stride * (size_t)os->height;
    memcpy(rb->data, os->bg_data, size);

    // Рисуем выделение поверх
    cairo_t *cr = cairo_create(rb->cairo_surface);

    // Затемнение всего экрана
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.25);
    cairo_paint(cr);

    // Рисуем выделение (если есть)
    if (os->app->is_selecting || (os->app->start_x != os->app->ptr_x || os->app->start_y != os->app->ptr_y)) {
        int x1 = min(os->app->start_x, os->app->ptr_x);
        int y1 = min(os->app->start_y, os->app->ptr_y);
        int x2 = max(os->app->start_x, os->app->ptr_x);
        int y2 = max(os->app->start_y, os->app->ptr_y);
        int w = x2 - x1, h = y2 - y1;
        if (w == 0 || h == 0)  {
            cairo_destroy(cr);
            return;
        }

        int stroke = 4;

        // Восстанавливаем область выделения (без затемнения)
        cairo_set_source_surface(cr, os->cairo_bg, 0, 0);
        cairo_rectangle(cr, x1, y1, w, h);
        cairo_fill(cr);

        // Рамка выделения
        cairo_rectangle(cr, x1, y1, w, h);
        cairo_push_group(cr);
        cairo_set_line_width(cr, stroke);
        cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0); // цвет обводки
        cairo_stroke_preserve(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_fill(cr);
        cairo_pop_group_to_source(cr);
        cairo_paint(cr);

        char text[64];
        sprintf(text, "%dx%d", w, h);

        cairo_text_extents_t extents;
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14.0);
        cairo_text_extents(cr, text, &extents);

        int text_padding = 4;

        int rect_x = x1, rect_y = y1 - (int)extents.height - stroke - text_padding * 2;
        if (rect_y < 0) {
            rect_y = y1;
        }

        int rect_w = (int)extents.width + text_padding * 2, rect_h = (int)extents.height + text_padding * 2;

        // Фон для текста
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.5);
        cairo_rectangle(cr, rect_x, rect_y, rect_w, rect_h);
        cairo_fill(cr);

        // Текст
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_move_to(cr,
                      rect_x + (int)(rect_w - (int)extents.width) / 2.0 - (int)extents.x_bearing,
                      rect_y + (int)(rect_h - (int)extents.height) / 2.0 - (int)extents.y_bearing);
        cairo_show_text(cr, text);
    }

    cairo_destroy(cr);

    // Отправляем буфер
    wl_surface_attach(os->wl_surface, rb->wl_buffer, 0, 0);
    wl_surface_damage_buffer(os->wl_surface, 0, 0, os->width, os->height);

    // Запрашиваем frame callback для синхронизации
    os->frame_callback = wl_surface_frame(os->wl_surface);
    wl_callback_add_listener(os->frame_callback, &frame_listener, os);

    wl_surface_commit(os->wl_surface);
}
