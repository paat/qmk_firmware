# Notes about this modification
The modification allows user to use keyboard as US keyboard in system where keyboard layout is configured as ET (Estonian).

## base repo


## switch layouts
Layout switching is mapped to M1 Key.

## host OS
The Mac/Win switch on the board selects the host: Mac position = Linux, Win position = Windows. The keys are the same in both; only OS-specific characters on the US layer (`` ` `` and `~`) are typed differently.

## build firmware
In QMK terminal:
`qmk compile -kb keychron/q3_pro/iso_encoder_se -km paat`

## password layer (hold M4)
- Type a numeric PIN (up to 16 digits) on the number row, then Enter. The PIN is never sent to the host. Unlocking takes a moment while the PIN is stretched.
- Releasing M4 before Enter discards the digits. The indicator key is white while digits are pending, red when locked, and a PIN-dependent colour when unlocked: an unfamiliar colour means a mistyped PIN.
- Letter keys type a password; Shift + letter types the password the old firmware produced (for rotating accounts; disable with `PW_LEGACY_DISABLE`).
- It locks on M4 + Esc, M4 + Enter with no digits, Win+L, 5 minutes without a key press, host sleep, and when the board wakes from battery sleep.
- Put your own salt in `secrets.h` (see `secrets.h.example`); it is gitignored.

Passwords can be recomputed without the keyboard:

```python
import hashlib, hmac
CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#.,"
def password(pin, salt, letter, suffix="!8zY", iterations=4096):
    master = hashlib.pbkdf2_hmac("sha256", pin.encode(), salt.encode(), iterations, 32)
    digest = hmac.new(master, letter.encode(), hashlib.sha256).digest()
    return "".join(CHARS[b % len(CHARS)] for b in digest[:12]) + suffix
```

## RGB sync: extend the lighting to other devices
The keyboard streams what its LEDs show to the PC, and `host/rgb_sync.py` paints it onto every device [OpenRGB](https://openrgb.org) can drive (mouse, mouse mat, motherboard, GPU...). Whatever the keyboard shows carries over: `RGB_MOD` effects, the layout colours (M1), brightness, on/off.

- Works in cable mode only; on Bluetooth the other devices keep their last colours.
- The board is sampled as 9 colours, left to right (`rgb_sync.c`). A device zone with several LEDs gets them spread across; a single LED gets the middle of the board.
- The password-layer indicator key is left out, so its PIN-dependent colour never reaches the PC.

Setup on Linux, after flashing: run `host/install.sh [options]`. It installs a udev rule for the keyboard's raw HID interface (sudo) and two systemd user services: an OpenRGB server and `rgb_sync.py`. `host/install.sh --remove` undoes it.

To try the PC side without the keyboard: `openrgb --server &` then `host/rgb_sync.py --demo`. Options for `rgb_sync.py`, given to `install.sh`:
- `--strip-leds N`: drive N LEDs on addressable (ARGB) headers whose strip length is not set in OpenRGB. Needed for anything plugged into a motherboard ARGB header, e.g. a water block: `host/install.sh --strip-leds 30`.
- `--skip NAME`: leave devices whose name contains NAME alone.
- `--brightness 0.6`: dim the other devices.

### Lighting from the PC
`host/kbd_light.py` paints colours over the keyboard's own effect: the whole board, nine left-to-right bands, or single LEDs by index. The other devices follow. Painted colours scale with the keyboard's brightness and stay until released, until the optional `--hold` seconds pass, or until the RGB key is pressed (that press only hands the lighting back). Caps lock and the password indicator stay on top.

    host/kbd_light.py color ff8800
    host/kbd_light.py --hold 5 led 0 ff0000
    host/kbd_light.py cpu
    host/kbd_light.py release

It is also a small Python module (`from kbd_light import Keyboard`) for your own programs. This direction needs the `raw_hid_receive_user` hook in `q3_pro.c`.

An ASUS graphics card whose stock cooler was replaced still shows up in OpenRGB, but its lighting chip drives nothing.
