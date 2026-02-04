#pragma once

#include "common.h"

/* Listener для seat */
extern const struct wl_seat_listener seat_listener;

/* Listener для pointer */
extern const struct wl_pointer_listener pointer_listener;

/* Listener для keyboard */
extern const struct wl_keyboard_listener keyboard_listener;
