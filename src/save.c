#include "save.h"
#include "cairo.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static cairo_status_t write_to_file(void *closure, const unsigned char *data,
                                    unsigned int length) {
    return fwrite(data, 1, length, (FILE *)closure) == length
        ? CAIRO_STATUS_SUCCESS
        : CAIRO_STATUS_WRITE_ERROR;
}

cairo_status_t save(cairo_surface_t *surface, struct app_state *app,
                    struct output_state *os) {
    if (!app || !os || !os->cairo_bg) {
        fprintf(stderr, "Missing state or data_device not ready\n");
        return CAIRO_STATUS_NULL_POINTER;
    }

    char path[1024] = { 0 };
    if (!app->f_path) {
        FILE *user_dir = popen("xdg-user-dir PICTURES", "r");
        if (user_dir) {
            if (fgets(path, sizeof(path), user_dir)) {
                path[strcspn(path, "\n")] = 0;
            }
            pclose(user_dir);
        } else {
            strcpy(path, ".");
        }

        time_t time_epoch = time(nullptr);
        size_t path_len = strlen(path);
        strftime(path + path_len, sizeof(path) - path_len - 1, "/%Y%m%d_%H%M%S_" PROJECT_NAME ".png", localtime(&time_epoch));
    } else if (strcmp(app->f_path, "-") != 0) {
        strcpy(path, app->f_path);
    } else {
        return cairo_surface_write_to_png_stream(surface, write_to_file, stdout);
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", path);
        return CAIRO_STATUS_WRITE_ERROR;
    }

    return cairo_surface_write_to_png_stream(surface, write_to_file, fp);
}

cairo_status_t crop_and_save(struct app_state *app, struct output_state *os,
                             int x1, int y1, int x2, int y2) {
    if (!app || !os || !os->cairo_bg) {
        fprintf(stderr, "Missing state or data_device not ready\n");
        return 1;
    }

    // Нормализация координат
    int nx1 = min(x1, x2), ny1 = min(y1, y2);
    int nx2 = max(x1, x2), ny2 = max(y1, y2);
    int w = nx2 - nx1, h = ny2 - ny1;

    if (w < 1 || h < 1) {
        fprintf(stderr, "Selection too small\n");
        return 1;
    }

    cairo_surface_t *cropped = cairo_image_surface_create_for_data(
        (unsigned char *)os->bg_data + ny1 * os->stride + nx1 * 4,
        CAIRO_FORMAT_ARGB32, w, h, os->stride);
    cairo_status_t st = save(cropped, app, os);
    cairo_surface_destroy(cropped);
    return st;
}
