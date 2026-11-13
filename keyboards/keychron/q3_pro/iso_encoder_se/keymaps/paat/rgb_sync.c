// Links the keyboard's lighting with the host over raw HID, both ways.
//
// Keyboard -> host: what the LEDs currently show, so the host can extend the
// lighting onto other RGB devices (host/rgb_sync.py). The board is split into
// RGB_SYNC_ZONES vertical bands, left to right. Colours are read back from
// the LED driver's buffer, so every effect, indicator and brightness change
// is covered without knowing about it here.
//
// Host -> keyboard: colours painted over the running effect, per zone or per
// LED (host/kbd_light.py). They scale with the keyboard's brightness setting.
//
// Every report is 32 bytes and starts with 0xC5:
//   keyboard -> host   C5 01 zones  R,G,B per zone
//   host -> keyboard   C5 10 hold   R,G,B per zone        paint all zones
//                      C5 11 hold start count  R,G,B...   paint LEDs start..
//                      C5 1F                              release
// `hold` is how many seconds the paint stays without a further report
// (0 = until released), so a host program that dies does not leave it stuck.

#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include "usb_descriptor.h"
#include "transport.h"
#include "rgb_sync.h"

#define RGB_SYNC_MAGIC 0xC5
#define RGB_SYNC_ZONES 9
#define RGB_SYNC_FRAME_MS 40       // at most 25 frames per second
#define RGB_SYNC_HEARTBEAT_MS 1000 // resend an unchanged frame this often
#define RGB_SYNC_HEADER 3

enum {
    RGB_SYNC_FRAME      = 0x01,
    RGB_SYNC_SET_ZONES  = 0x10,
    RGB_SYNC_SET_LEDS   = 0x11,
    RGB_SYNC_RELEASE    = 0x1F,
};

_Static_assert(RGB_SYNC_HEADER + RGB_SYNC_ZONES * 3 <= RAW_EPSIZE, "rgb_sync frame does not fit a raw HID report");

// Defined in drivers/led/snled27351.c: the colours last written to the LEDs.
extern uint8_t g_pwm_buffer[SNLED27351_DRIVER_COUNT][192];

static uint8_t  host_rgb[RGB_MATRIX_LED_COUNT][3];
static bool     host_painted[RGB_MATRIX_LED_COUNT]; // LEDs the host has taken over
static bool     host_active  = false;
static uint32_t host_last    = 0; // time of the last paint report
static uint8_t  host_hold_s  = 0;

static uint8_t led_zone(uint8_t led) {
    return MIN((uint16_t)g_led_config.point[led].x * RGB_SYNC_ZONES / 225, RGB_SYNC_ZONES - 1);
}

// ---------------------------------------------------------- host -> keyboard

bool rgb_sync_host_release(void) {
    if (!host_active) {
        return false;
    }
    // Effects that only touch a few LEDs per frame (the rain ones) would leave
    // the host's colours behind: clear them and start the effect afresh.
    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        if (host_painted[i]) {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
    }
    memset(host_painted, 0, sizeof(host_painted));
    host_active = false;
    rgb_matrix_mode_noeeprom(rgb_matrix_get_mode());
    return true;
}

static void host_paint(uint8_t led, const uint8_t *rgb) {
    memcpy(host_rgb[led], rgb, 3);
    host_painted[led] = true;
}

// Called by the board for raw HID reports it does not use itself.
void raw_hid_receive_user(uint8_t *data, uint8_t length) {
    if (length < RAW_EPSIZE || data[0] != RGB_SYNC_MAGIC) {
        return;
    }
    switch (data[1]) {
        case RGB_SYNC_SET_ZONES:
            for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
                host_paint(i, &data[3 + led_zone(i) * 3]);
            }
            break;
        case RGB_SYNC_SET_LEDS: {
            uint8_t start = data[3];
            uint8_t count = MIN(data[4], (RAW_EPSIZE - 5) / 3);
            for (uint8_t i = 0; i < count && start + i < RGB_MATRIX_LED_COUNT; i++) {
                host_paint(start + i, &data[5 + i * 3]);
            }
            break;
        }
        case RGB_SYNC_RELEASE:
            rgb_sync_host_release();
            return;
        default:
            return;
    }
    host_active = true;
    host_hold_s = data[2];
    host_last   = timer_read32();
}

void rgb_sync_host_render(uint8_t led_min, uint8_t led_max) {
    if (!host_active) {
        return;
    }
    uint8_t val = rgb_matrix_get_val();
    for (uint8_t i = led_min; i < led_max; i++) {
        if (host_painted[i]) {
            rgb_matrix_set_color(i, (uint16_t)host_rgb[i][0] * val / 255, (uint16_t)host_rgb[i][1] * val / 255, (uint16_t)host_rgb[i][2] * val / 255);
        }
    }
}

// ---------------------------------------------------------- keyboard -> host

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
        uint8_t        zone   = led_zone(i);

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

    if (host_active && (get_transport() != TRANSPORT_USB || (host_hold_s && timer_elapsed32(host_last) > host_hold_s * 1000UL))) {
        rgb_sync_host_release();
    }

    if (get_transport() != TRANSPORT_USB || timer_elapsed32(last_sample) < RGB_SYNC_FRAME_MS) {
        return;
    }
    last_sample = timer_read32();

    uint8_t frame[RAW_EPSIZE] = {RGB_SYNC_MAGIC, RGB_SYNC_FRAME, RGB_SYNC_ZONES};
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
