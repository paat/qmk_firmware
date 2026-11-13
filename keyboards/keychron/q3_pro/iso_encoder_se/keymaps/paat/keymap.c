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
#include "pwkdf.h"
#include "sha256.h" // old non-standard hash, kept only for legacy passwords

// Private values live in secrets.h next to this file (gitignored, see
// secrets.h.example). Without it the published defaults below are used.
#if __has_include("secrets.h")
#    include "secrets.h"
#endif
#ifndef PASSWORD_SALT
#    define PASSWORD_SALT "Kynnivare5"
#endif
#ifndef PASSWORD_SUFFIX
#    define PASSWORD_SUFFIX "!8zY"
#endif

const char *password_salt = PASSWORD_SALT;
const char ALLOWED_PW_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#.,";
const char *mandatoryPrefix = PASSWORD_SUFFIX;

#ifndef PW_KDF_ITERATIONS
#    define PW_KDF_ITERATIONS 4096 // PBKDF2 rounds, run once per unlock
#endif
#ifndef PW_AUTOLOCK_MS
#    define PW_AUTOLOCK_MS (5UL * 60 * 1000) // lock after this long without a key press
#endif
#define PW_PIN_MAX_DIGITS 16 // further digits are ignored
#define PW_LENGTH 12         // generated characters before the suffix

// Shift + letter types the password the pre-hardening firmware produced, so
// accounts can be rotated. Those passwords are derivable from the public repo:
// define PW_LEGACY_DISABLE in secrets.h once rotation is done.
#ifndef PW_LEGACY_DISABLE
#    define PW_LEGACY_PIN "123456"
#    define PW_LEGACY_SALT "Kynnivare5"
#    define PW_LEGACY_SUFFIX "!8zY"
#endif

static char     pw_pin_buf[PW_PIN_MAX_DIGITS]; // digits typed so far; never sent to the host
static uint8_t  pw_pin_len       = 0;
static uint8_t  pw_master[PW_HASH_LEN];        // PBKDF2(PIN, salt); meaningful only while unlocked
static bool     pw_unlocked      = false;
static uint32_t pw_last_activity = 0;          // time of the last key press, for the idle auto-lock
static uint32_t pw_last_tick     = 0;          // last timer value seen, to notice the timer restarting
static uint8_t  pw_fingerprint   = 0;          // colour hint derived from the master key


// clang-format off
// The Mac/Win switch on the board picks the host OS profile. Both profiles
// have the same keys; they differ only in how OS-specific characters are typed.
enum layers{
  MAC_BASE, // switch at Mac: Linux host
  MAC_FN,
  WIN_BASE, // switch at Win: Windows host
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
  KC_PW_Z
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_93_iso(
        KC_MUTE,        KC_ESC,             KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,     KC_F12,   KC_PSCR,  KC_CTANA, RGB_MOD,
                        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,     KC_BSPC,  KC_INS,   KC_HOME,  KC_PGUP,
        KC_M1,          KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,              KC_DEL,   KC_END,   KC_PGDN,
        _______,        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,  KC_NUHS,    KC_ENT,
        MO(M3_PW),        KC_LSFT,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,              KC_RSFT,            KC_UP,
        MO(M4_MY),        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  KC_RWIN,  MO(MAC_FN), KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT),

    [MAC_FN] = LAYOUT_93_iso(
        RGB_TOG,  _______,            KC_BRID,  KC_BRIU,  KC_TASK,  KC_FILE,  RGB_VAD,  RGB_VAI,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,    _______,  _______,  RGB_TOG,
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
                  _______,      KC_PW_1,  KC_PW_2,  KC_PW_3,  KC_PW_4,  KC_PW_5,  KC_PW_6,  KC_PW_7,  KC_PW_8,  KC_PW_9,  KC_PW_0,     _______,  _______, _______,    _______,   _______,  _______,
        _______,          _______,   KC_PW_Q,     KC_PW_W,     KC_PW_E,     KC_PW_R,     KC_PW_T,     KC_PW_Y,     KC_PW_U,     KC_PW_I,     KC_PW_O,     KC_PW_P,     _______,  _______,              _______,   _______,   _______,
        _______,        _______,  KC_PW_A,     KC_PW_S,     KC_PW_D,     KC_PW_F,     KC_PW_G,     KC_PW_H,     KC_PW_J,     KC_PW_K,     KC_PW_L,     _______,  _______,  _______,    KC_PW_ENT,
        _______,        _______,  _______,  KC_PW_Z,     KC_PW_X,     KC_PW_C,     KC_PW_V,     KC_PW_B,     KC_PW_N,     KC_PW_M,     _______,  _______,   _______,              _______,            _______,
        _______,  _______,  _______,  _______,                                _______,                                _______,  _______,  _______,    _______,  _______,  _______,  _______),
};

#define LUT_BIT(lut, pos) ((pgm_read_byte(&((lut)[(pos) / 8])) >> ((pos) % 8)) & 0x01)

// send_char() with a pause after pressing Shift or AltGr; without it some
// hosts miss the modifier and type the unshifted character.
static void send_char_slow(char ascii_code) {
    uint8_t c = (uint8_t)ascii_code;
    if (c > 127) {
        return;
    }
    uint8_t keycode    = pgm_read_byte(&ascii_to_keycode_lut[c]);
    bool    is_shifted = LUT_BIT(ascii_to_shift_lut, c);
    bool    is_altgred = LUT_BIT(ascii_to_altgr_lut, c);
    bool    is_dead    = LUT_BIT(ascii_to_dead_lut, c);

    if (is_shifted) {
        register_code(KC_LEFT_SHIFT);
        wait_ms(10);
    }
    if (is_altgred) {
        register_code(KC_RIGHT_ALT);
        wait_ms(10);
    }
    tap_code(keycode);
    if (is_altgred) {
        unregister_code(KC_RIGHT_ALT);
    }
    if (is_shifted) {
        unregister_code(KC_LEFT_SHIFT);
    }
    if (is_dead) {
        tap_code(KC_SPACE);
    }
}

// Plain text only (no SS_TAP-style escapes).
static void send_string_slow(const char *string, uint8_t interval) {
    while (*string) {
        send_char_slow(*string++);
        wait_ms(interval);
    }
}

static void pw_lock(void) {
    pw_wipe(pw_master, sizeof(pw_master));
    pw_wipe(pw_pin_buf, sizeof(pw_pin_buf));
    pw_pin_len     = 0;
    pw_fingerprint = 0;
    pw_unlocked    = false;
}

// Stretch the entered PIN into the master key. Blocks while PBKDF2 runs.
// An empty entry locks instead.
static void pw_unlock(void) {
    if (pw_pin_len == 0) {
        pw_lock();
        return;
    }
    pw_pbkdf2_sha256((const uint8_t *)pw_pin_buf, pw_pin_len, (const uint8_t *)password_salt, strlen(password_salt), PW_KDF_ITERATIONS, pw_master);
    pw_wipe(pw_pin_buf, sizeof(pw_pin_buf));
    pw_pin_len = 0;

    // One byte of a separate hash picks the indicator colour, so a mistyped
    // PIN shows up as an unfamiliar colour.
    uint8_t hash[PW_HASH_LEN];
    pw_hmac_sha256(pw_master, sizeof(pw_master), (const uint8_t *)"fingerprint", 11, hash);
    pw_fingerprint = hash[0];
    pw_wipe(hash, sizeof(hash));

    pw_unlocked      = true;
    pw_last_activity = timer_read32();
}

// Lock after PW_AUTOLOCK_MS without a key press. The timer restarts from zero
// when the board wakes from low-power sleep, so a timer that went backwards
// means it slept: lock then too.
static void pw_autolock_task(void) {
    uint32_t now = timer_read32();
    if (pw_unlocked && (now < pw_last_tick || now - pw_last_activity > PW_AUTOLOCK_MS)) {
        pw_lock();
    }
    pw_last_tick = now;
}

// Releasing M4 before Enter discards the digits typed so far.
layer_state_t layer_state_set_user(layer_state_t state) {
    if (!layer_state_cmp(state, M3_PW) && pw_pin_len > 0) {
        pw_wipe(pw_pin_buf, sizeof(pw_pin_buf));
        pw_pin_len = 0;
    }
    return state;
}

void housekeeping_task_user(void) {
    pw_autolock_task();
}

// The USB host went to sleep.
void suspend_power_down_user(void) {
    pw_lock();
}

static void type_password(const uint8_t *hash, const char *suffix) {
    size_t length = strlen(ALLOWED_PW_CHARS);
    for (uint8_t i = 0; i < PW_LENGTH; i++) {
        wait_ms(10);
        send_char_slow(ALLOWED_PW_CHARS[hash[i] % length]);
        wait_ms(10);
    }
    send_string_slow(suffix, 30);
}

// password = chars(HMAC-SHA256(master, key)) + suffix
static void generate_password(char key) {
    uint8_t hash[PW_HASH_LEN];
    pw_hmac_sha256(pw_master, sizeof(pw_master), (const uint8_t *)&key, 1, hash);
    type_password(hash, mandatoryPrefix);
    pw_wipe(hash, sizeof(hash));
}

#ifndef PW_LEGACY_DISABLE
// Exactly what firmware built before the PIN fix typed: it ignored the
// entered PIN and always used these values. (The short-lived version that
// hashed the entered PIN with the old algorithm was never flashed.)
static void generate_legacy_password(char key) {
    char     input[sizeof(PW_LEGACY_PIN PW_LEGACY_SALT) + 1];
    uint32_t state[8];
    uint8_t  hash[32];

    snprintf(input, sizeof(input), "%s%s%c", PW_LEGACY_PIN, PW_LEGACY_SALT, key);
    sha256_init(state);
    sha256_update(state, (const uint8_t *)input, strlen(input));
    sha256_final(hash, state);
    type_password(hash, PW_LEGACY_SUFFIX);
}
#endif

void PWX(char pressed_key) {
    if (!pw_unlocked) {
        send_string_slow("NoLuck!", 10);
        return;
    }

    uint8_t mods = get_mods();
    clear_mods(); // a held modifier must not change the typed characters
#ifndef PW_LEGACY_DISABLE
    if (mods & MOD_MASK_SHIFT) {
        generate_legacy_password(pressed_key);
    } else
#endif
    {
        generate_password(pressed_key);
    }
    set_mods(mods);
}

// True when the Mac/Win switch is at Mac, which this keymap uses for Linux hosts.
static bool host_is_linux(void) {
    return get_highest_layer(default_layer_state) == MAC_BASE;
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

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  static bool win_key_pressed = false;

  pw_autolock_task();
  if (record->event.pressed) {
      pw_last_activity = timer_read32();
  }

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
        if (host_is_linux()) {
            // xkb "ee": ` is AltGr + the acute key. ~ exists only as a dead
            // key (Shift + the caron key), completed with Space.
            if (is_shift_pressed()) {
                tap_with_modifier(KC_GRV, KC_LSFT);
                tap_wo_modifier(KC_SPC);
            } else {
                tap_with_modifier(KC_EQL, KC_RALT);
            }
        } else {
            // Windows: Alt+numpad codes (these do nothing on Linux)
            if (is_shift_pressed()) {
                tap_ascii_code(KC_P1, KC_P2, KC_P6);
            } else {
                tap_ascii_code(KC_P0, KC_P9, KC_P6);
            }
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
            send_string_slow("mymail", 5);
            tap_with_modifier(KC_2, KC_RALT);
            send_string_slow("gmail.com", 5);
      }
      return false;
    case KC_MY_PH:
      if (record->event.pressed) {
            send_string_slow("myphone", 5);
      }
      return false;
    case KC_MY_IK:
      if (record->event.pressed) {
            send_string_slow("mypersonalcode", 5);
      }
      return false;
    case KC_MY_ST:
      if (record->event.pressed) {
            send_string_slow("My streetaddress", 5);
      }
      return false;
    case KC_MY_ZIP:
      if (record->event.pressed) {
            send_string_slow("myzip", 5);
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
          // Win+L locks the computer: lock the password layer with it
          pw_lock();
      }
      return true;
    case KC_PW_LOCK:
      if (record->event.pressed) {
        pw_lock();
      }
      return false;
    case KC_PW_0 ... KC_PW_9:
      if (record->event.pressed && pw_pin_len < PW_PIN_MAX_DIGITS) {
        pw_pin_buf[pw_pin_len++] = '0' + (keycode - KC_PW_0);
      }
      return false;
    case KC_PW_ENT:
      if (record->event.pressed) {
        pw_unlock();
      }
      return false;
    case KC_PW_A ... KC_PW_Z:
      if (record->event.pressed) {
        PWX('a' + (keycode - KC_PW_A));
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
    // Password layer state: white = PIN digits pending, red = locked,
    // any other colour = unlocked (the colour depends on the PIN).
    if (pw_pin_len > 0) {
        rgb_matrix_set_color(g_led_config.matrix_co[4][17], RGB_WHITE);
    } else if (pw_unlocked) {
#ifdef PW_FINGERPRINT_DISABLE
        rgb_matrix_set_color(g_led_config.matrix_co[4][17], RGB_YELLOW);
#else
        // hue 30..225: yellow through green and blue to purple, never red
        RGB rgb = hsv_to_rgb((HSV){30 + (uint8_t)((uint16_t)pw_fingerprint * 195 / 255), 255, 255});
        rgb_matrix_set_color(g_led_config.matrix_co[4][17], rgb.r, rgb.g, rgb.b);
#endif
    } else {
        rgb_matrix_set_color(g_led_config.matrix_co[4][17], RGB_RED);
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
