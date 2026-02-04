#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>
#include <cairo/cairo.h>
#include <xkbcommon/xkbcommon.h>

#include "protocols/cursor-shape-v1-client-protocol.h"
#include "protocols/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "protocols/wlr-screencopy-unstable-v1-client-protocol.h"

/* Константы */
#define RENDER_BUFFERS_COUNT 3

/* Forward declarations */
struct app_state;
struct output_state;

/* Структура буфера для отрисовки */
struct render_buffer {
    struct wl_buffer *wl_buffer;
    void *data;
    cairo_surface_t *cairo_surface;
    bool busy;
};

/* Состояние отдельного монитора */
struct output_state {
    struct app_state *app;
    struct wl_output *wl_output;
    struct wl_surface *wl_surface;
    struct zwlr_layer_surface_v1 *layer_surface;

    // Данные скриншота
    struct zwlr_screencopy_frame_v1 *sc_frame;
    struct wl_buffer *bg_buffer;
    void *bg_data;
    cairo_surface_t *cairo_bg;
    int width, height, stride;
    uint32_t format;  // Добавлено для хранения формата

    // Пул буферов для отрисовки
    struct render_buffer buffers[RENDER_BUFFERS_COUNT];

    struct wl_callback *frame_callback;
    bool frame_scheduled;
    bool dirty;

    bool configured;
    struct wl_list link;
};

/* Глобальное состояние приложения */
struct app_state {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wp_cursor_shape_manager_v1 *cursor_shape_manager;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zwlr_screencopy_manager_v1 *screencopy_manager;

    // Флаги
    char *f_output;
    char *f_path;

    // Клавиатура
    struct xkb_context *xkb_ctx;
    struct xkb_state *xkb_state;
    struct wl_keyboard *keyboard;

    // Мышь
    struct wl_pointer *pointer;
    struct output_state *pointer_focus_output;
    int ptr_x, ptr_y;
    int start_x, start_y;
    bool is_selecting;

    struct wl_list outputs;
    bool running;
    bool screencopy_called;
};

void stub();

int max(int a, int b);

int min(int a, int b);
