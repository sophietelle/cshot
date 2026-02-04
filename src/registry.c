#include "registry.h"
#include "input.h"
#include "screencopy.h"
#include <stdlib.h>
#include <string.h>

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t) {
    struct app_state *app = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        app->compositor = wl_registry_bind(registry, name,
            &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        app->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        app->seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
        wl_seat_add_listener(app->seat, &seat_listener, app);
    } else if (strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
        app->cursor_shape_manager = wl_registry_bind(registry, name,
            &wp_cursor_shape_manager_v1_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        app->layer_shell = wl_registry_bind(registry, name,
            &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        app->screencopy_manager = wl_registry_bind(registry, name,
            &zwlr_screencopy_manager_v1_interface, 1);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct output_state *os = calloc(1, sizeof(struct output_state));
        if (!os) {
            fprintf(stderr, "Failed to allocate output_state\n");
            return;
        }

        os->app = app;
        os->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
        wl_output_add_listener(os->wl_output, &output_listener, os);
        wl_list_insert(&app->outputs, &os->link);

        /*int current_output = app->output_counter++;
        if (app->f_output >= 0 && current_output != app->f_output) {
            return;
        }

        struct output_state *os = calloc(1, sizeof(struct output_state));
        if (!os) {
            fprintf(stderr, "Failed to allocate output_state\n");
            return;
        }

        os->app = app;
        os->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 4);
        wl_list_insert(&app->outputs, &os->link);

        start_screencopy(os);*/
    }
}

const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = stub,
};
