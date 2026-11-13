// !!! DO NOT ADD #pragma once !!! //

// Step 1.
// Declare custom effects using the RGB_MATRIX_EFFECT macro
// (note the lack of semicolon after the macro!)
RGB_MATRIX_EFFECT(default_key_map)
RGB_MATRIX_EFFECT(et_us_key_map)
RGB_MATRIX_EFFECT(pc_view)

// Step 2.
// Define effects inside the `RGB_MATRIX_CUSTOM_EFFECT_IMPLS` ifdef block
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

#include "rgb_sync.h"

// e.g: A simple effect, self-contained within a single method
static bool default_key_map(effect_params_t* params) {
  RGB_MATRIX_USE_LIMITS(led_min, led_max);
  for (uint8_t i = led_min; i < led_max; i++) {
    if (i == g_led_config.matrix_co[1][17]) {
        rgb_matrix_set_color(i, RGB_GREEN);
    }
    else if (i == g_led_config.matrix_co[5][17]) {
        rgb_matrix_set_color(i, 255,0,255);
    }
    else {
        rgb_matrix_set_color(i, RGB_BLUE);
    }
  }
  return rgb_matrix_check_finished_leds(led_max);
}

static bool et_us_key_map(effect_params_t* params) {
  RGB_MATRIX_USE_LIMITS(led_min, led_max);
  for (uint8_t i = led_min; i < led_max; i++) {
    if (i == g_led_config.matrix_co[1][17]) {
        rgb_matrix_set_color(i, RGB_BLUE);
    }
    else if (i == g_led_config.matrix_co[5][17]) {
        rgb_matrix_set_color(i, RGB_MAGENTA);
    }
    else {
        rgb_matrix_set_color(i, RGB_GREEN);
    }
  }
  return rgb_matrix_check_finished_leds(led_max);
}

// Lighting generated on the PC (rgb_sync.c): bars growing from the bottom row
// up, or a canvas of colours, both blended across the board. Brightness
// follows the usual RGB keys. Without data the board glows dimly, in a hue
// that tells the views apart.
static bool pc_view(effect_params_t* params) {
  RGB_MATRIX_USE_LIMITS(led_min, led_max);
  const rgb_sync_pc_data_t *pc = rgb_sync_pc_data();
  HSV hsv = rgb_matrix_config.hsv;
  for (uint8_t i = led_min; i < led_max; i++) {
    uint8_t  x    = g_led_config.point[i].x; // 0 (left) .. 224
    uint8_t  y    = g_led_config.point[i].y; // 0 (top) .. 64
    uint16_t pos  = (uint16_t)x * (RGB_SYNC_ZONES - 1) * 256 / 225;
    uint8_t  zone = pos >> 8; // this LED lies between zone and zone + 1
    uint8_t  mix  = pos & 0xFF;
    RGB rgb;
    if (!pc) {
      rgb = rgb_matrix_hsv_to_rgb((HSV){rgb_sync_pc_view() * 50, 255, scale8(hsv.v, 40)});
    } else if (pc->kind == PC_DATA_BARS) {
      const uint8_t *levels = &pc->values[1];
      uint8_t level = lerp8by8(levels[zone], levels[zone + 1], mix);
      // how high the level must be to reach this row; a soft edge above it
      uint8_t threshold  = (uint16_t)(64 - MIN(y, 64)) * 200 / 64;
      uint8_t brightness = level > threshold ? MIN((uint16_t)(level - threshold) * 5, 255) : 0;
      uint8_t hue = pc->values[0] == PC_BARS_METER ? 85 - scale8(level, 85) : hsv.h + x / 2;
      rgb = rgb_matrix_hsv_to_rgb((HSV){hue, hsv.s, scale8(hsv.v, brightness)});
    } else {
      const uint8_t *from = &pc->values[zone * 3];
      rgb.r = scale8(lerp8by8(from[0], from[3], mix), hsv.v);
      rgb.g = scale8(lerp8by8(from[1], from[4], mix), hsv.v);
      rgb.b = scale8(lerp8by8(from[2], from[5], mix), hsv.v);
    }
    rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
  }
  return rgb_matrix_check_finished_leds(led_max);
}


// // e.g: A more complex effect, relying on external methods and state, with
// // dedicated init and run methods
// static uint8_t some_global_state;
// static void my_cool_effect2_complex_init(effect_params_t* params) {
//   some_global_state = 1;
// }
// static bool my_cool_effect2_complex_run(effect_params_t* params) {
//   RGB_MATRIX_USE_LIMITS(led_min, led_max);
//   for (uint8_t i = led_min; i < led_max; i++) {
//     rgb_matrix_set_color(i, 0xff, some_global_state++, 0xff);
//   }
//   return rgb_matrix_check_finished_leds(led_max);
// }
// static bool my_cool_effect2(effect_params_t* params) {
//   if (params->init) my_cool_effect2_complex_init(params);
//   return my_cool_effect2_complex_run(params);
// }

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
