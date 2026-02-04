#pragma once

#include "common.h"

cairo_status_t save(cairo_surface_t *surface, struct app_state *app,
                    struct output_state *os);

cairo_status_t crop_and_save(struct app_state *app, struct output_state *os,
                             int x1, int y1, int x2, int y2);
