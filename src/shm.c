#include "shm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <wayland-client.h>

/* Callback для освобождения буфера */
static void buffer_release(void *data, struct wl_buffer *) {
    struct render_buffer *rb = data;
    rb->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

int allocate_shm_file(size_t size) {
    int retries = 100;

    do {
        char name[] = "/wl_shm-XXXXXX";
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long r = ts.tv_nsec;

        // Генерируем случайное имя
        for (int i = 0; i < 6; ++i) {
            name[8 + i] = 'A' + (r & 15) + (r & 16) * 2;
            r >>= 5;
        }

        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);

            // Устанавливаем размер файла
            int ret;
            do {
                ret = ftruncate(fd, (off_t)size);
            } while (ret < 0 && errno == EINTR);

            if (ret < 0) {
                close(fd);
                return -1;
            }

            return fd;
        }
    } while (retries-- > 0 && errno == EEXIST);

    return -1;
}

struct wl_buffer *create_buffer(struct app_state *app, int width, int height,
                               int stride, uint32_t format, void **data_out) {
    if (!app || !app->shm || !data_out) {
        return nullptr;
    }

    size_t size = (size_t)stride * (size_t)height;
    int fd = allocate_shm_file(size);
    if (fd < 0) {
        fprintf(stderr, "Failed to allocate shm file\n");
        return nullptr;
    }

    void *data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return nullptr;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, (int32_t)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height,
                                                         stride, format);

    wl_shm_pool_destroy(pool);
    close(fd);

    if (!buffer) {
        munmap(data, size);
        return nullptr;
    }

    *data_out = data;
    return buffer;
}

bool init_render_buffer(struct app_state *app, struct render_buffer *rb,
                        int width, int height, int stride, uint32_t format) {
    if (!app || !rb) {
        return false;
    }

    size_t size = (size_t)stride * (size_t)height;
    int fd = allocate_shm_file(size);
    if (fd < 0) {
        return false;
    }

    rb->data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (rb->data == MAP_FAILED) {
        close(fd);
        return false;
    }

    memset(rb->data, 0, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, (int32_t)size);
    rb->wl_buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
    wl_shm_pool_destroy(pool);
    close(fd);

    // Добавляем слушатель для отслеживания освобождения буфера
    wl_buffer_add_listener(rb->wl_buffer, &buffer_listener, rb);

    // Создаем Cairo поверхность
    rb->cairo_surface = cairo_image_surface_create_for_data(
        (unsigned char *)rb->data, CAIRO_FORMAT_ARGB32, width, height, stride);

    rb->busy = false;
    return true;
}

void cleanup_render_buffer(struct render_buffer *rb, size_t size) {
    if (!rb) {
        return;
    }

    if (rb->cairo_surface) {
        cairo_surface_destroy(rb->cairo_surface);
        rb->cairo_surface = nullptr;
    }

    if (rb->wl_buffer) {
        wl_buffer_destroy(rb->wl_buffer);
        rb->wl_buffer = nullptr;
    }

    if (rb->data && rb->data != MAP_FAILED) {
        munmap(rb->data, size);
        rb->data = nullptr;
    }

    rb->busy = false;
}
