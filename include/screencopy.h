#pragma once

#include "common.h"

/* Listener для output */
extern const struct wl_output_listener output_listener;

void start_screencopy(struct output_state *os);

void setup_layer_surface(struct output_state *os);
