#include "input.h"
#include "save.h"
#include "render.h"
#include <math.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-util.h>

/* ===== POINTER (мышь) ===== */

static void pointer_enter(void *data, struct wl_pointer * pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t sx, wl_fixed_t sy) {
    struct app_state *app = data;
    struct output_state *os;

    wl_list_for_each(os, &app->outputs, link) {
        if (os->wl_surface == surface) {
            app->pointer_focus_output = os;
            app->ptr_x = round(wl_fixed_to_double(sx));
            app->ptr_y = round(wl_fixed_to_double(sy));
            break;
        }
    }

    if (app->cursor_shape_manager) {
        struct wp_cursor_shape_device_v1 *device =
            wp_cursor_shape_manager_v1_get_pointer(app->cursor_shape_manager, pointer);
        wp_cursor_shape_device_v1_set_shape(device, serial,
             WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR);
        wp_cursor_shape_device_v1_destroy(device);
    }
}

static void pointer_motion(void *data, struct wl_pointer *, uint32_t,
                           wl_fixed_t sx, wl_fixed_t sy) {
    struct app_state *app = data;
    app->ptr_x = round(wl_fixed_to_double(sx));
    app->ptr_y = round(wl_fixed_to_double(sy));

    if (app->is_selecting && app->pointer_focus_output) {
        render_output(app->pointer_focus_output);
    }
}

static void pointer_button(void *data, struct wl_pointer *, uint32_t,
                           uint32_t, uint32_t button, uint32_t state) {
    struct app_state *app = data;

    if (button == 0x110) { // BTN_LEFT
        if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
            app->is_selecting = true;
            app->start_x = app->ptr_x;
            app->start_y = app->ptr_y;
        } else {
            app->is_selecting = false;

            /*if (app->pointer_focus_output && crop_and_save(app, app->pointer_focus_output,
                app->start_x, app->start_y,
                app->ptr_x, app->ptr_y) == 0) {
                if (app->f_path) {
                    app->running = false;
                    printf("Saved to %s\n", app->f_path);
                } else {
                    cleanup_ui(app);
                    printf("Process is still alive to serve clipboard data\n");
                }
            }*/

            if (app->pointer_focus_output) {
                crop_and_save(app, app->pointer_focus_output,
                              app->start_x, app->start_y,
                              app->ptr_x, app->ptr_y);
                app->running = false;
            }
        }
    }
}

const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = stub,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = stub,
    .frame = stub,
    .axis_source = stub,
    .axis_stop = stub,
    .axis_discrete = stub,
};

/* ===== KEYBOARD (клавиатура) ===== */

static void keyboard_keymap(void *data, struct wl_keyboard *,
                            uint32_t format, int32_t fd, uint32_t size) {
    struct app_state *app = data;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_shm = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_shm == MAP_FAILED) {
        close(fd);
        return;
    }

    struct xkb_keymap *keymap = xkb_keymap_new_from_string(
        app->xkb_ctx, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(map_shm, size);
    close(fd);

    if (app->xkb_state) {
        xkb_state_unref(app->xkb_state);
    }

    if (!keymap) {
        return;
    }

    app->xkb_state = xkb_state_new(keymap);
    xkb_keymap_unref(keymap);
}

static void keyboard_key(void *data, struct wl_keyboard *, uint32_t,
                         uint32_t, uint32_t key, uint32_t state) {
    struct app_state *app = data;

    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, key + 8);
        if (sym == XKB_KEY_Escape) {
            app->running = false;
        }
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *, uint32_t,
                               uint32_t mods_depressed, uint32_t mods_latched,
                               uint32_t mods_locked, uint32_t group) {
    struct app_state *app = data;
    xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
}

const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = stub,
    .leave = stub,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = stub,
};

/* ===== SEAT ===== */

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t capabilities) {
    struct app_state *app = data;

    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        app->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(app->pointer, &pointer_listener, app);
    }

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        app->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(app->keyboard, &keyboard_listener, app);
    }
}

const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = stub
};
