// Streams what the keyboard LEDs currently show to the host, so the host can
// extend the lighting onto other RGB devices (see host/rgb_sync.py).
//
// The board is split into RGB_SYNC_ZONES vertical bands, left to right. Each
// frame is one 32-byte raw HID report:
//   [0] 0xC5  [1] 0x01  [2] zone count  [3..] R,G,B per zone
// Colours are read back from the LED driver's buffer, so every effect,
// indicator and brightness change is covered without knowing about it here.

#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include "usb_descriptor.h"
#include "transport.h"
#include "rgb_sync.h"

#define RGB_SYNC_ZONES 9
#define RGB_SYNC_FRAME_MS 40       // at most 25 frames per second
#define RGB_SYNC_HEARTBEAT_MS 1000 // resend an unchanged frame this often
#define RGB_SYNC_HEADER 3

_Static_assert(RGB_SYNC_HEADER + RGB_SYNC_ZONES * 3 <= RAW_EPSIZE, "rgb_sync frame does not fit a raw HID report");

// Defined in drivers/led/snled27351.c: the colours last written to the LEDs.
extern uint8_t g_pwm_buffer[SNLED27351_DRIVER_COUNT][192];

static void rgb_sync_sample(uint8_t *out) {
    uint32_t sum[RGB_SYNC_ZONES][3] = {0}; // colour sums, weighted by brightness
    uint16_t weight[RGB_SYNC_ZONES] = {0};
    uint8_t  brightest[RGB_SYNC_ZONES] = {0};
    uint8_t  count[RGB_SYNC_ZONES] = {0};

    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (i == PW_INDICATOR_LED) {
            continue;
        }
        snled27351_led_t led;
        memcpy_P(&led, &g_snled27351_leds[i], sizeof(led));
        const uint8_t *pwm    = g_pwm_buffer[led.driver];
        uint8_t        rgb[3] = {pwm[led.r], pwm[led.g], pwm[led.b]};
        uint8_t        w      = MAX(rgb[0], MAX(rgb[1], rgb[2]));
        uint8_t        zone   = MIN((uint16_t)g_led_config.point[i].x * RGB_SYNC_ZONES / 225, RGB_SYNC_ZONES - 1);

        for (uint8_t c = 0; c < 3; c++) {
            sum[zone][c] += (uint32_t)rgb[c] * w;
        }
        weight[zone] += w;
        brightest[zone] = MAX(brightest[zone], w);
        count[zone]++;
    }

    for (uint8_t z = 0; z < RGB_SYNC_ZONES; z++, out += 3) {
        out[0] = out[1] = out[2] = 0;
        if (weight[z] == 0) {
            continue;
        }
        // Hue: mean of the lit LEDs, so dark keys do not wash it out.
        uint8_t rgb[3], peak = 0;
        for (uint8_t c = 0; c < 3; c++) {
            rgb[c] = sum[z][c] / weight[z];
            peak   = MAX(peak, rgb[c]);
        }
        if (peak == 0) {
            continue;
        }
        // Brightness: halfway between the zone's mean and its brightest LED,
        // so a single lit key (reactive effects) still shows up.
        uint16_t level = (weight[z] / count[z] + brightest[z]) / 2;
        for (uint8_t c = 0; c < 3; c++) {
            out[c] = (uint16_t)rgb[c] * level / peak;
        }
    }
}

void rgb_sync_task(void) {
    static uint32_t last_sample = 0;
    static uint32_t last_sent   = 0;
    static uint8_t  sent[RAW_EPSIZE];

    if (get_transport() != TRANSPORT_USB || timer_elapsed32(last_sample) < RGB_SYNC_FRAME_MS) {
        return;
    }
    last_sample = timer_read32();

    uint8_t frame[RAW_EPSIZE] = {0xC5, 0x01, RGB_SYNC_ZONES};
    rgb_sync_sample(frame + RGB_SYNC_HEADER);

    if (memcmp(frame, sent, sizeof(frame)) == 0 && timer_elapsed32(last_sent) < RGB_SYNC_HEARTBEAT_MS) {
        return;
    }
    // With nobody reading on the host this returns at once (the USB core
    // short-circuits an endpoint that timed out), so typing is not delayed.
    raw_hid_send(frame, sizeof(frame));
    memcpy(sent, frame, sizeof(sent));
    last_sent = timer_read32();
}
