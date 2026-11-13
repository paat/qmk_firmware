#pragma once

#include <stdbool.h>
#include <stdint.h>

// Password-layer state indicator (see keymap.c). Its colour hints at the PIN,
// so rgb_sync never includes it in what is sent to the host.
#define PW_INDICATOR_LED (g_led_config.matrix_co[4][17])

// Streams a summary of the live LED colours to the host over raw HID, for
// host/rgb_sync.py to mirror onto other RGB devices. Call from housekeeping.
void rgb_sync_task(void);

// Draws the colours the host has painted (host/kbd_light.py) over the running
// effect. Call first in rgb_matrix_indicators_advanced_user.
void rgb_sync_host_render(uint8_t led_min, uint8_t led_max);

// Drops everything the host has painted. Returns whether there was anything.
bool rgb_sync_host_release(void);
