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
#include "sha256.h"

static uint32_t pin_input = 0; // for processing ongoing input
static uint32_t pin = 0;  // store pin after enter key
const char *password_salt = "Kynnivare5";
const char ALLOWED_PW_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#.,";
const char *mandatoryPrefix = "!8zY";


// clang-format off
enum layers{
  MAC_BASE,
  MAC_FN,
  WIN_BASE,
  WIN_FN,
  WIN_ET_US, //Emulate UK layout in system where ET layout is default
  M4_MY, //my personal autotype macros
  M3_PW, //pw autotype macros
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
  KC_M1,
  KC_MY_IK, //personal code
  KC_MY_EM, //email
  KC_MY_PH, //phone
  KC_MY_ST, //My street address
  KC_MY_ZIP, //my zip code
  KC_PW_LOCK,
  KC_MACRO3,
  KC_PW_0,
  KC_PW_1,
  KC_PW_2,
  KC_PW_3,
  KC_PW_4,
  KC_PW_5,
  KC_PW_6,
  KC_PW_7,
  KC_PW_8,
  KC_PW_9,
  KC_PW_ENT,
  KC_PW_A,
  KC_PW_B,
  KC_PW_C,
  KC_PW_D,
  KC_PW_E,
  KC_PW_F,
  KC_PW_G,
  KC_PW_H,
  KC_PW_I,
  KC_PW_J,
  KC_PW_K,
  KC_PW_L,
  KC_PW_M,
  KC_PW_N,
  KC_PW_O,
  KC_PW_P,
  KC_PW_Q,
  KC_PW_R,
  KC_PW_S,
  KC_PW_T,
  KC_PW_U,
  KC_PW_V,
  KC_PW_W,
  KC_PW_X,
  KC_PW_Y,
  KC_PW_Z,
  KC_PW_PRNT
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
        MO(M3_PW),        KC_LSFT,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RSFT,            KC_UP,
        MO(M4_MY),        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  KC_RWIN,  MO(WIN_FN), KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

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
        KC_M1,    KC_TAB,       KC_Q,           KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,           KC_P,           KC_LBRC_ET_US,  KC_RBRC_ET_US,            KC_DEL,   KC_END,   KC_PGDN,
        _______,  KC_CAPS,      KC_A,           KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,           KC_SCLN_ET_US,  KC_QUOT_ET_US,  KC_NUHS_ET_US,        KC_ENT,
        MO(M3_PW),  KC_LSFT,      KC_NUBS_ET_US,  KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM_ET_US,  KC_DOT_ET_US,   KC_SLSH_ET_US,                        KC_RSFT,            KC_UP,
        MO(M4_MY),  KC_LCTL,      KC_LWIN,        KC_LALT,                                KC_SPC,                                       KC_RALT,         KC_RWIN,        MO(WIN_FN),     KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [M4_MY] = LAYOUT_93_iso(
        _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,
                  _______,  KC_MY_ZIP,  _______,  KC_MY_IK,  _______,  KC_MY_PH,  _______,  _______,  _______,  _______,  _______,  _______,  _______,    _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  KC_MY_EM,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,  _______,  _______,
        _______,  _______,  _______, _______,  _______,  _______,  _______,  _______,  _______,  _______,  KC_MY_ST,  _______,  _______,  _______,    _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,              _______,            _______,
        _______,  _______,  _______,  _______,                                _______,                                _______,  _______,  _______,    _______,  _______,  _______,  _______),

    [M3_PW] = LAYOUT_93_iso(
        _______,  KC_PW_LOCK,          _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,   _______,     _______,   _______,  _______, _______,
                  KC_PW_PRNT,   KC_PW_1,  KC_PW_2,  KC_PW_3,  KC_PW_4,  KC_PW_5,  KC_PW_6,  KC_PW_7,  KC_PW_8,  KC_PW_9,  KC_PW_0,     _______,  _______, _______,    _______,   _______,  _______,
        _______,          _______,   KC_PW_Q,     KC_PW_W,     KC_PW_E,     KC_PW_R,     KC_PW_T,     KC_PW_Y,     KC_PW_U,     KC_PW_I,     KC_PW_O,     KC_PW_P,     _______,  _______,              _______,   _______,   _______,
        _______,        _______,  KC_PW_A,     KC_PW_S,     KC_PW_D,     KC_PW_F,     KC_PW_G,     KC_PW_H,     KC_PW_J,     KC_PW_K,     KC_PW_L,     _______,  _______,  _______,    KC_PW_ENT,
        _______,        _______,  _______,  KC_PW_Z,     KC_PW_X,     KC_PW_C,     KC_PW_V,     KC_PW_B,     KC_PW_N,     KC_PW_M,     _______,  _______,   _______,              _______,            _______,
        _______,  _______,  _______,  _______,                                _______,                                _______,  _______,  _______,    _______,  _______,  _______,  _______),
};

void hash_function(const char* input, uint8_t* output) {
    //uint8_t hash[32];  // Buffer for the raw hash output
    uint32_t state[8]; // State for the SHA-256 computation
    //char buf[3];       // Temporary buffer for formatting each byte

    // Initialize the SHA-256 state
    sha256_init(state);

    // Update the hash with the input data
    sha256_update(state, (const uint8_t *)input, strlen(input));

    // Finalize the hash
    //sha256_final(hash, state);
    sha256_final(output, state);

    // Convert hash bytes to a hexadecimal string
    //output[0] = '\0';  // Start with an empty string
    //for (int i = 0; i < 32; i++) {
    //    snprintf(buf, sizeof(buf), "%02x", hash[i]);
    //    strcat(output, buf);  // Append the hex string
    //}
}

void type_password(uint8_t* hash) {
    int char_count = 0; // Initialize a counter for the characters
    size_t length = strlen(ALLOWED_PW_CHARS);
    while (char_count < 12) { // Only loop for the first 12 characters
        uint8_t i = hash[char_count] % length;
        char ascii_code = pgm_read_byte(&ALLOWED_PW_CHARS[i]);
        if (!ascii_code) break; // Break if the end of the string is reached prematurely

        wait_ms(10);
        send_char(ascii_code);
        ++char_count; // Increment the character counter

        // interval
        {
            uint8_t ms = 10;
            while (ms--)
                wait_ms(1);
        }
    }
    send_string_with_delay(mandatoryPrefix, 30);
}


void generate_password(char pressed_key) {
    long pin = 123456; // Assuming you have a pin number. Make sure to define or fetch it appropriately.
    char pin_str[12];  // Buffer for the PIN string
    snprintf(pin_str, sizeof(pin_str), "%ld", pin);  // Convert PIN number to string

    // Allocate buffer for the salted input (PIN string + salt + 1 for the pressed key)
    size_t input_size = strlen(pin_str) + strlen(password_salt) + 2; // +2 for the pressed key and the null terminator
    char salted_input[input_size];
    memset(salted_input, 0, input_size);  // Clear the buffer

    strncpy(salted_input, pin_str, sizeof(salted_input) - 1);  // Start with the PIN string
    strncat(salted_input, password_salt, sizeof(salted_input) - strlen(salted_input) - 1);  // Safely append the salt

    // Append the pressed key to the salted input, ensuring there is room for the null terminator
    size_t current_length = strlen(salted_input);
    if (current_length < sizeof(salted_input) - 1) {
        salted_input[current_length] = pressed_key;
        salted_input[current_length + 1] = '\0'; // Ensure null termination
    }

    // Buffer to hold the hashed output
    uint8_t hash[32];  // Adjust size based on the hash function output
    hash_function(salted_input, hash);

    // Type out the hashed password
    type_password(hash);
}

void PWX(char pressed_key) {
    if (pin > 0) {
        generate_password(pressed_key);
    }
    else {
        send_string_with_delay("NoLuck!", 10);
    }
}

bool is_shift_pressed(void) {
    return get_mods() & (MOD_BIT(KC_LSFT) | MOD_BIT(KC_RSFT));
}

//tap with specified modifier exclusivelly
void tap_with_modifier(uint16_t keycode, uint16_t modifier) {
    uint8_t mods = get_mods();
    clear_mods();
    register_mods(MOD_BIT(modifier)); // Press specified modifier
    wait_ms(5);
    tap_code(keycode);               // Then press the specified keycode
    wait_ms(5);
    unregister_mods(MOD_BIT(modifier)); // Release specified modifier
    set_mods(mods);
}

void tap_ascii_code(uint16_t key1, uint16_t key2, uint16_t key3) {
    uint8_t mods = get_mods();
    clear_mods();
    register_mods(MOD_BIT(KC_LALT)); // Press Left ALT
    wait_ms(5);
    tap_code(key1);
    tap_code(key2);
    tap_code(key3);
    wait_ms(5);
    unregister_mods(MOD_BIT(KC_LALT)); // Release specified modifier
    set_mods(mods);
}

//tap without any modifiers
void tap_wo_modifier(uint16_t keycode) {
    uint8_t mods = get_mods();
    clear_mods();
    wait_ms(5);
    tap_code(keycode);
    wait_ms(5);
    set_mods(mods);
}

void tap_int_value(int32_t value) {
    if (value < 0) {
        tap_code(KC_MINS);  // Handle negative numbers
        value = -value;     // Make the value positive
    }

    uint32_t divisor = value;
    while (divisor > 0) {
        int digit = divisor % 10;
        divisor = divisor / 10;
        switch (digit) {
            case 0:
                tap_wo_modifier(KC_0);
                break;
            case 1:
                tap_wo_modifier(KC_1);
                break;
            case 2:
                tap_wo_modifier(KC_2);
                break;
            case 3:
                tap_wo_modifier(KC_3);
                break;
            case 4:
                tap_wo_modifier(KC_4);
                break;
            case 5:
                tap_wo_modifier(KC_5);
                break;
            case 6:
                tap_wo_modifier(KC_6);
                break;
            case 7:
                tap_wo_modifier(KC_7);
                break;
            case 8:
                tap_wo_modifier(KC_8);
                break;
            case 9:
                tap_wo_modifier(KC_9);
                break;
            default:
                send_string_with_delay("Undefined", 10);
                break;

        }

    }
}


void tap_char(char c) {
    if (c >= '0' && c <= '9') {
        tap_int_value(c - '0');
    } else if (c >= 'a' && c <= 'z') {
        tap_wo_modifier(KC_A);
    } else if (c >= 'A' && c <= 'Z') {
        tap_with_modifier(KC_A, KC_LSFT);
    } else {
        tap_with_modifier(KC_1, KC_LSFT); //adds !
    }
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  static bool win_key_pressed = false;

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
    case KC_2_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_2, KC_RALT);
        } else {
            tap_code(KC_2);
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
    case KC_6_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_QUOT, KC_RALT);
        } else {
            tap_code(KC_6);
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
    case KC_8_ET_US:
      if (record->event.pressed) {
        if (is_shift_pressed()) {
            tap_with_modifier(KC_NUHS, KC_LSFT);
        } else {
            tap_code(KC_8);
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
    case KC_MY_EM:
      if (record->event.pressed) {
            send_string_with_delay("mymail", 5);
            tap_with_modifier(KC_2, KC_RALT);
            send_string_with_delay("gmail.com", 5);
      }
      return false;
    case KC_MY_PH:
      if (record->event.pressed) {
            send_string_with_delay("myphone", 5);
      }
      return false;
    case KC_MY_IK:
      if (record->event.pressed) {
            send_string_with_delay("mypersonalcode", 5);
      }
      return false;
    case KC_MY_ST:
      if (record->event.pressed) {
            send_string_with_delay("My streetaddress", 5);
      }
      return false;
    case KC_MY_ZIP:
      if (record->event.pressed) {
            send_string_with_delay("myzip", 5);
      }
      return false;
    case KC_LWIN:
    case KC_RWIN:
      if (record->event.pressed) {
          win_key_pressed = true;
      } else {
          win_key_pressed = false;
      }
      return true;
    case KC_L:
      if (record->event.pressed && win_key_pressed) {
          // Lock the keyboard or switch layers when Win+L is pressed
          pin = 0;
          pin_input = 0;
      }
      return true;
    case KC_PW_0 ... KC_PW_9:
      if (record->event.pressed) {
        pin_input = pin_input * 10 + (keycode - KC_PW_0);
      }
      return false;
    case KC_PW_ENT:
      if (record->event.pressed) {
        pin=pin_input;
        pin_input = 0;
      }
      return false;
    case KC_PW_PRNT:
      if (record->event.pressed) {
        tap_int_value(pin);
      }
      return false;
    case KC_PW_A:
      if (record->event.pressed) {
        PWX('a');
      }
      return false;
    case KC_PW_B:
      if (record->event.pressed) {
        PWX('b');
      }
      return false;
    case KC_PW_C:
      if (record->event.pressed) {
        PWX('c');
      }
      return false;
    case KC_PW_D:
      if (record->event.pressed) {
        PWX('d');
      }
      return false;
    case KC_PW_E:
      if (record->event.pressed) {
        PWX('e');
      }
      return false;
    case KC_PW_F:
      if (record->event.pressed) {
        PWX('f');
      }
      return false;
    case KC_PW_G:
      if (record->event.pressed) {
        PWX('g');
      }
      return false;
    case KC_PW_H:
      if (record->event.pressed) {
        PWX('h');
      }
      return false;
    case KC_PW_I:
      if (record->event.pressed) {
        PWX('i');
      }
      return false;
    case KC_PW_J:
      if (record->event.pressed) {
        PWX('j');
      }
      return false;
    case KC_PW_K:
      if (record->event.pressed) {
        PWX('k');
      }
      return false;
    case KC_PW_L:
      if (record->event.pressed) {
        PWX('l');
      }
      return false;
    case KC_PW_M:
      if (record->event.pressed) {
        PWX('m');
      }
      return false;
    case KC_PW_N:
      if (record->event.pressed) {
        PWX('n');
      }
      return false;
    case KC_PW_O:
      if (record->event.pressed) {
        PWX('o');
      }
      return false;
    case KC_PW_P:
      if (record->event.pressed) {
        PWX('p');
      }
      return false;
    case KC_PW_Q:
      if (record->event.pressed) {
        PWX('q');
      }
      return false;
    case KC_PW_R:
      if (record->event.pressed) {
        PWX('r');
      }
      return false;
    case KC_PW_S:
      if (record->event.pressed) {
        PWX('s');
      }
      return false;
    case KC_PW_T:
      if (record->event.pressed) {
        PWX('t');
      }
      return false;
    case KC_PW_U:
      if (record->event.pressed) {
        PWX('u');
      }
      return false;
    case KC_PW_V:
      if (record->event.pressed) {
        PWX('v');
      }
      return false;
    case KC_PW_W:
      if (record->event.pressed) {
        PWX('w');
      }
      return false;
    case KC_PW_X:
      if (record->event.pressed) {
        PWX('x');
      }
      return false;
    case KC_PW_Y:
      if (record->event.pressed) {
        PWX('y');
      }
      return false;
    case KC_PW_Z:
      if (record->event.pressed) {
        PWX('z');
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
        //set caps lock key as red
        rgb_matrix_set_color(g_led_config.matrix_co[3][0], 255, 0, 0);
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
    [M4_MY] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [M3_PW] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif // ENCODER_MAP_ENABLE
