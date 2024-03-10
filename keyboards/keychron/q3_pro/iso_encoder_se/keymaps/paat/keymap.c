/* Copyright 2023 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H

// clang-format off
enum layers{
  MAC_BASE,
  MAC_FN,
  WIN_BASE,
  WIN_FN,
  WIN_ET_US //Emulate UK layout in system where ET layout is default
};

enum custom_keycodes {
  KC_LBRC_ET_US = NEW_SAFE_RANGE,   // [
  KC_RBRC_ET_US,                // ]
  KC_COMM_ET_US,                // ,
  KC_DOT_ET_US,                  // .
  KC_EQL_ET_US,                  // =
  KC_MINS_ET_US,                 // -
  KC_0_ET_US,
  KC_9_ET_US,
  KC_8_ET_US,
  KC_7_ET_US,
  KC_6_ET_US,
  KC_4_ET_US,
  KC_2_ET_US,
  KC_GRV_ET_US,
  KC_SCLN_ET_US,  //;
  KC_QUOT_ET_US,  //"
  KC_NUHS_ET_US,
  KC_SLSH_ET_US,
  KC_NUBS_ET_US,
  KC_M1
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_93_iso(
        KC_MUTE,  KC_ESC,             KC_BRID,  KC_BRIU,  KC_MCTL,  KC_LPAD,  RGB_VAD,  RGB_VAI,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,    KC_VOLU,  KC_SNAP,  KC_SIRI,  RGB_MOD,
                  KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,     KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,
        _______,  KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,              KC_DEL,   KC_END,   KC_PGDN,
        _______,  KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,  KC_NUHS,    KC_ENT,
        _______,  KC_LSFT,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RSFT,            KC_UP,
        _______,  KC_LCTL,  KC_LOPT,  KC_LCMD,                                KC_SPC,                                 KC_RCMD,  KC_ROPT,  MO(MAC_FN), KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [MAC_FN] = LAYOUT_93_iso(
        RGB_TOG,  _______,            KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,     KC_F12,   _______,  _______,  RGB_TOG,
                  _______,  BT_HST1,  BT_HST2,  BT_HST3,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,
        _______,  RGB_TOG,  RGB_MOD,  RGB_VAI,  RGB_HUI,  RGB_SAI,  RGB_SPI,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  _______,  RGB_RMOD, RGB_VAD,  RGB_HUD,  RGB_SAD,  RGB_SPD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  BAT_LVL,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  _______,  _______,                                _______,                                _______,  _______,  _______,    _______,  _______,  _______,  _______),

    [WIN_BASE] = LAYOUT_93_iso(
        KC_MUTE,        KC_ESC,             KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,     KC_F12,   KC_PSCR,  KC_CTANA, RGB_MOD,
                        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,     KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,
        KC_M1,          KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,              KC_DEL,   KC_END,   KC_PGDN,
        _______,        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,  KC_NUHS,    KC_ENT,
        _______,        KC_LSFT,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RSFT,            KC_UP,
        _______,        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  KC_RWIN,  MO(WIN_FN), KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [WIN_FN] = LAYOUT_93_iso(
        RGB_TOG,  _______,            KC_BRID,  KC_BRIU,  KC_TASK,  KC_FILE,  RGB_VAD,  RGB_VAI,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,    _______,  _______,  RGB_TOG,
                  _______,  BT_HST1,  BT_HST2,  BT_HST3,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,
        _______,  RGB_TOG,  RGB_MOD,  RGB_VAI,  RGB_HUI,  RGB_SAI,  RGB_SPI,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  _______,  RGB_RMOD, RGB_VAD,  RGB_HUD,  RGB_SAD,  RGB_SPD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  BAT_LVL,  NK_TOGG,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  _______,  _______,                                _______,                                _______,  _______,  _______,    _______,  _______,  _______,  _______),

    [WIN_ET_US] = LAYOUT_93_iso(
        KC_MUTE,  KC_ESC,                       KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,          KC_F9,          KC_F10,         KC_F11,         KC_F12,   KC_PSCR,  KC_CTANA, RGB_MOD,
                  KC_GRV_ET_US, KC_1,           KC_2_ET_US, KC_3,       KC_4_ET_US, KC_5,       KC_6_ET_US, KC_7_ET_US, KC_8_ET_US, KC_9_ET_US,     KC_0_ET_US,     KC_MINS_ET_US,  KC_EQL_ET_US,   KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,
        KC_M1,    KC_TAB,       KC_P,           KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,           KC_P,           KC_LBRC_ET_US,  KC_RBRC_ET_US,            KC_DEL,   KC_END,   KC_PGDN,
        _______,  KC_CAPS,      KC_A,           KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,           KC_SCLN_ET_US,  KC_QUOT_ET_US,  KC_NUHS_ET_US,        KC_ENT,
        _______,  KC_LSFT,      KC_NUBS_ET_US,  KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM_ET_US,  KC_DOT_ET_US,   KC_SLSH_ET_US,                        KC_RSFT,            KC_UP,
        _______,  KC_LCTL,      KC_LWIN,        KC_LALT,                                KC_SPC,                                       KC_RALT,         KC_RWIN,        MO(WIN_FN),     KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),
};

bool is_shift_pressed(void) {
    return get_mods() & (MOD_BIT(KC_LSFT) | MOD_BIT(KC_RSFT));
}

//tap with specified modifier exclusivelly
void tap_with_modifier(uint16_t keycode, uint16_t modifier) {
    uint8_t mods = get_mods();
    clear_mods();
    register_mods(MOD_BIT(modifier)); // Press specified modifier
    tap_code(keycode);               // Then press the specified keycode
    unregister_mods(MOD_BIT(modifier)); // Release specified modifier
    set_mods(mods);
}

void tap_ascii_code(uint16_t key1, uint16_t key2, uint16_t key3) {
    uint8_t mods = get_mods();
    clear_mods();
    register_mods(MOD_BIT(KC_LALT)); // Press Left ALT
    tap_code(key1);
    tap_code(key2);
    tap_code(key3);
    unregister_mods(MOD_BIT(KC_LALT)); // Release specified modifier
    set_mods(mods);
}

//tap without any modifiers
void tap_wo_modifier(uint16_t keycode) {
    uint8_t mods = get_mods();
    clear_mods();
    tap_code(keycode);
    set_mods(mods);
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  switch (keycode) {
    case KC_M1:
      if (record->event.pressed) {
        layer_invert(WIN_ET_US);
        if (layer_state_is(WIN_ET_US)) {
            rgb_matrix_mode(RGB_MATRIX_CUSTOM_et_us_key_map);
        }
        else {
            rgb_matrix_mode(RGB_MATRIX_CUSTOM_default_key_map);
        }
      }
      return false;
    case KC_LBRC_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_7, KC_RALT);
        } else {
            tap_with_modifier(KC_8, KC_RALT);
        }
      }
      return false;
    case KC_RBRC_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_0, KC_RALT);
        } else {
            tap_with_modifier(KC_9, KC_RALT);
        }
      }
      return false;
    case KC_COMM_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_wo_modifier(KC_NUBS);
        } else {
            tap_code(KC_COMM);
        }
      }
      return false;
    case KC_DOT_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_NUBS, KC_LSFT);
        } else {
            tap_code(KC_DOT);
        }
      }
      return false;
    case KC_EQL_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_wo_modifier(KC_MINS);
        } else {
            tap_with_modifier(KC_0, KC_LSFT);
        }
      }
      return false;
    case KC_MINS_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_SLSH, KC_LSFT);
        } else {
            tap_code(KC_SLSH);
        }
      }
      return false;
    case KC_0_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_9, KC_LSFT);
        } else {
            tap_code(KC_0);
        }
      }
      return false;
    case KC_9_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_8, KC_LSFT);
        } else {
            tap_code(KC_9);
        }
      }
      return false;
    case KC_8_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_NUHS, KC_LSFT);
        } else {
            tap_code(KC_8);
        }
      }
      return false;
    case KC_7_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_6, KC_LSFT);
        } else {
            tap_code(KC_7);
        }
      }
      return false;
    case KC_6_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_QUOT, KC_RALT);
        } else {
            tap_code(KC_6);
        }
      }
      return false;
    case KC_4_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_4, KC_RALT);
        } else {
            tap_code(KC_4);
        }
      }
      return false;
    case KC_2_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_2, KC_RALT);
        } else {
            tap_code(KC_2);
        }
      }
      return false;
    case KC_GRV_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_ascii_code(KC_P1, KC_P2, KC_P6);
        } else {
            tap_ascii_code(KC_P0, KC_P9, KC_P6);
        }
      }
      return false;
    case KC_SCLN_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_DOT, KC_LSFT);
        } else {
            tap_with_modifier(KC_COMM, KC_LSFT);
        }
      }
      return false;
    case KC_QUOT_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_2, KC_LSFT);
        } else {
            tap_code(KC_NUHS);
        }
      }
      return false;
    case KC_NUHS_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_NUBS, KC_RALT);
        } else {
            tap_with_modifier(KC_MINS, KC_RALT);
        }
      }
      return false;
    case KC_SLSH_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_MINS, KC_LSFT);
        } else {
            tap_with_modifier(KC_7, KC_LSFT);
        }
      }
      return false;
    case KC_NUBS_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_NUBS, KC_RALT);
        } else {
            tap_with_modifier(KC_MINS, KC_RALT);
        }
      }
      return false;
      }

  return true; // Process all other keycodes normally
}

void keyboard_post_init_user(void) {
    rgb_matrix_mode(RGB_MATRIX_CUSTOM_default_key_map);
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    if (host_keyboard_led_state().caps_lock) {
        for (uint8_t i = led_min; i < led_max; i++) {
            if (g_led_config.flags[i] & LED_FLAG_KEYLIGHT) {
                rgb_matrix_set_color(i, RGB_RED);
            }
        }
    }
    return false;
}


#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [MAC_BASE] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [MAC_FN]   = { ENCODER_CCW_CW(RGB_VAD, RGB_VAI)},
    [WIN_BASE] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [WIN_FN]   = { ENCODER_CCW_CW(RGB_VAD, RGB_VAI)},
    [WIN_ET_US] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif // ENCODER_MAP_ENABLE
