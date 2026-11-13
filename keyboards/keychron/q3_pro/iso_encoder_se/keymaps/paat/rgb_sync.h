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

#define RGB_SYNC_ZONES 9

// Lighting the host generates and the pc_view effect (rgb_matrix_user.inc)
// draws. The keyboard picks the view; the host is told which in every frame
// and sends the data for it, as bars or as a canvas.
enum rgb_sync_pc_view {
    PC_VIEW_NONE,   // the keyboard's own lighting
    PC_VIEW_MUSIC,  // spectrum of what the PC plays
    PC_VIEW_SYSTEM, // CPU, memory and GPU load
    PC_VIEW_CODING, // what the coding agents are doing
    PC_VIEW_SCREEN, // colours of the lower edge of the screen
    PC_VIEW_COUNT,
};

enum rgb_sync_pc_kind {
    PC_DATA_BARS,   // values: style, then a level (0-255) per zone
    PC_DATA_CANVAS, // values: R,G,B per zone
};

enum rgb_sync_bar_style {
    PC_BARS_RAINBOW, // hue runs across the board
    PC_BARS_METER,   // green when low, red when high
};

typedef struct {
    uint8_t kind;
    uint8_t values[RGB_SYNC_ZONES * 3];
} rgb_sync_pc_data_t;

void    rgb_sync_set_pc_view(uint8_t view);
uint8_t rgb_sync_pc_view(void);

// What the host last sent for the current view, or NULL when nothing arrived
// recently (no host program, or the view was only just selected).
const rgb_sync_pc_data_t *rgb_sync_pc_data(void);

// Drops everything the host has painted. Returns whether there was anything.
bool rgb_sync_host_release(void);
