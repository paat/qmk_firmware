#pragma once

// Password-layer state indicator (see keymap.c). Its colour hints at the PIN,
// so rgb_sync never includes it in what is sent to the host.
#define PW_INDICATOR_LED (g_led_config.matrix_co[4][17])

// Streams a summary of the live LED colours to the host over raw HID, for
// host/rgb_sync.py to mirror onto other RGB devices. Call from housekeeping.
void rgb_sync_task(void);
