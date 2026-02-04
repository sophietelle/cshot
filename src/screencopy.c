#include "screencopy.h"
#include "save.h"
#include "shm.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

static void output_handle_name(void *data, struct wl_output *, const char *name) {
    struct output_state *os = data;
    if (os->app->f_output && strcmp(name, os->app->f_output) != 0) {
        return;
    }
    start_screencopy(os);
    os->app->screencopy_called = true;
}

const struct wl_output_listener output_listener = {
    .geometry = stub,
    .mode = stub,
    .done = stub,
    .scale = stub,
    .name = output_handle_name,
    .description = stub
};

/* Callbacks для screencopy frame */
static void screencopy_buffer(void *data, struct zwlr_screencopy_frame_v1 *frame,
                              uint32_t format, uint32_t width, uint32_t height,
                              uint32_t stride) {
    struct output_state *os = data;
    os->width = (int)width;
    os->height = (int)height;
    os->stride = (int)stride;
    os->format = format;

    // Создаем буфер для получения скриншота
    os->bg_buffer = create_buffer(os->app, os->width, os->height, os->stride,
                                  format, &os->bg_data);
    if (!os->bg_buffer) {
        fprintf(stderr, "Failed to create background buffer\n");
        zwlr_screencopy_frame_v1_destroy(frame);
        os->app->running = false;
        return;
    }

    zwlr_screencopy_frame_v1_copy(frame, os->bg_buffer);
}

static void screencopy_ready(void *data, struct zwlr_screencopy_frame_v1 *frame,
                             uint32_t, uint32_t, uint32_t) {
    struct output_state *os = data;
    zwlr_screencopy_frame_v1_destroy(frame);
    os->sc_frame = nullptr;

    // Создаем Cairo поверхность для фона
    os->cairo_bg = cairo_image_surface_create_for_data(
        (unsigned char *)os->bg_data, CAIRO_FORMAT_ARGB32,
        os->width, os->height, os->stride);

    if (os->app->f_output) {
        /*if (save(os->cairo_bg, os->app, os) != 0) {
            os->app->running = false;
        } else if (os->app->f_path) {
            os->app->running = false;
            printf("Saved to %s\n", os->app->f_path);
        } else {
            cleanup_ui(os->app);
            printf("Process is still alive to serve clipboard data\n");
        }*/
        save(os->cairo_bg, os->app, os);
        os->app->running = false;

        return;
    }

    // Инициализируем буферы для отрисовки
    for (int i = 0; i < RENDER_BUFFERS_COUNT; i++) {
        if (!init_render_buffer(os->app, &os->buffers[i], os->width, os->height,
                                os->stride, WL_SHM_FORMAT_ARGB8888)) {
            fprintf(stderr, "Failed to init render buffer %d\n", i);
        }
    }

    // Настраиваем layer surface
    setup_layer_surface(os);
}

static void screencopy_failed(void *data, struct zwlr_screencopy_frame_v1 *frame) {
    struct output_state *os = data;
    fprintf(stderr, "Screencopy failed for output\n");
    zwlr_screencopy_frame_v1_destroy(frame);
    os->app->running = false;
}

static const struct zwlr_screencopy_frame_v1_listener screencopy_frame_listener = {
    .buffer = screencopy_buffer,
    .flags = stub,
    .ready = screencopy_ready,
    .failed = screencopy_failed,
};

void start_screencopy(struct output_state *os) {
    if (!os || !os->app || !os->app->screencopy_manager || !os->wl_output) {
        return;
    }

    os->sc_frame = zwlr_screencopy_manager_v1_capture_output(
        os->app->screencopy_manager, 0, os->wl_output);

    zwlr_screencopy_frame_v1_add_listener(os->sc_frame, &screencopy_frame_listener, os);
}

/* Callbacks для layer surface */
static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface,
                                    uint32_t serial, uint32_t, uint32_t) {
    struct output_state *os = data;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    os->configured = true;
    render_output(os);
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *) {
    struct output_state *os = data;
    os->configured = false;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

void setup_layer_surface(struct output_state *os) {
    if (!os || !os->app || !os->app->compositor || !os->app->layer_shell) {
        return;
    }

    os->wl_surface = wl_compositor_create_surface(os->app->compositor);
    os->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        os->app->layer_shell, os->wl_surface, os->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, PROJECT_NAME);

    zwlr_layer_surface_v1_set_anchor(os->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(os->layer_surface, -1);

    zwlr_layer_surface_v1_set_keyboard_interactivity(os->layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);

    zwlr_layer_surface_v1_add_listener(os->layer_surface, &layer_surface_listener, os);

    wl_surface_commit(os->wl_surface);
}
