#pragma once

#include "common.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Создать файл в shared memory заданного размера
 */
int allocate_shm_file(size_t size);

/**
 * Создать Wayland буфер
 */
struct wl_buffer *create_buffer(struct app_state *app, int width, int height,
                                int stride, uint32_t format, void **data_out);

/**
 * Инициализировать буфер для отрисовки
 */
bool init_render_buffer(struct app_state *app, struct render_buffer *rb,
                       int width, int height, int stride, uint32_t format);

/**
 * Освободить ресурсы буфера отрисовки
 */
void cleanup_render_buffer(struct render_buffer *rb, size_t size);
