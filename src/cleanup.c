#include "cleanup.h"
#include "shm.h"
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <wayland-util.h>

void cleanup_ui(struct app_state *app) {
    if (!app) return;

    if (app->display) {
        struct output_state *os, *tmp;
        wl_list_for_each_safe(os, tmp, &app->outputs, link) {
            wl_list_remove(&os->link);
            if (os->layer_surface) zwlr_layer_surface_v1_destroy(os->layer_surface);
            if (os->wl_surface) wl_surface_destroy(os->wl_surface);
            if (os->frame_callback) wl_callback_destroy(os->frame_callback);

            size_t buffer_size = (size_t)os->stride * (size_t)os->height;
            for (int i = 0; i < RENDER_BUFFERS_COUNT; i++) {
                cleanup_render_buffer(&os->buffers[i], buffer_size);
            }

            if (os->cairo_bg) cairo_surface_destroy(os->cairo_bg);
            if (os->bg_buffer) wl_buffer_destroy(os->bg_buffer);
            if (os->bg_data && os->bg_data != MAP_FAILED) munmap(os->bg_data, buffer_size);
            if (os->sc_frame) zwlr_screencopy_frame_v1_destroy(os->sc_frame);
            if (os->wl_output) wl_output_destroy(os->wl_output);
            free(os);
        }
    }

    // Удаляем устройства ввода (это безопасно, метод destroy там был всегда)
    if (app->pointer) { wl_pointer_destroy(app->pointer); app->pointer = nullptr; }
    if (app->keyboard) { wl_keyboard_destroy(app->keyboard); app->keyboard = nullptr; }

    // Очищаем XKB
    if (app->xkb_state) { xkb_state_unref(app->xkb_state); app->xkb_state = nullptr; }
    if (app->xkb_ctx) { xkb_context_unref(app->xkb_ctx); app->xkb_ctx = nullptr; }

    if (app->cursor_shape_manager) {
        wp_cursor_shape_manager_v1_destroy(app->cursor_shape_manager);
        app->cursor_shape_manager = nullptr;
    }

    // Просто зануляем указатели, чтобы не использовать их случайно
    app->compositor = nullptr;
    app->shm = nullptr;
    app->seat = nullptr;
    app->layer_shell = nullptr;
    app->screencopy_manager = nullptr;
    app->registry = nullptr;

    if (app->display) wl_display_flush(app->display);
}

void cleanup_app(struct app_state *app) {
    if (!app) return;

    cleanup_ui(app);

    if (app->display) { wl_display_disconnect(app->display); app->display = nullptr; }
}
