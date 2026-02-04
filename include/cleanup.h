#pragma once

#include "common.h"

/**
 * Очистить поверхности и буферы
 */
void cleanup_ui(struct app_state *app);

/**
 * Очистить все ресурсы приложения
 */
void cleanup_app(struct app_state *app);
