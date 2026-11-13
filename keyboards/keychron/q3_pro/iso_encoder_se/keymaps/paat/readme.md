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

## lighting views (RGB key)
The RGB key steps through five views; the board's stock animations are left out of the firmware (`config.h`). Brightness works in all of them: hold the RGB key and turn the knob (a tap of the RGB key alone steps to the next view), or Fn + turn.

1. **Static**: the layout colours (blue = default, green = US-on-ET layer, toggled with M1).
2. **Music**: spectrum of what the PC plays, bass at the left, bars growing from the bottom row. Fn+E/D shift the hue.
3. **System**: three meters, CPU load, memory in use, GPU load, green when low and red when high.
4. **Coding**: what the coding agents do. Left half Claude, right half Codex: a running pulse while working, blinking yellow when one waits for you, green for a few minutes when one has finished, dim otherwise.
5. **Screen**: the colours along the lower part of the screen.

On the PC the current view shows as an icon in the system tray (hover for the name), and a short notification appears whenever it changes (`host/desktop.py`; KDE Plasma and other trays that speak StatusNotifierItem).

Views 2-5 are generated on the PC by `host/rgb_sync.py` (see below), which only works while the view is showing; without it, or on Bluetooth, the board glows dimly in a colour of its own per view. Extra needs: NumPy and `parec` for music; `host/install_hooks.py` once for coding (adds hooks to `~/.claude/settings.json` and `~/.codex/hooks.json`; in Codex, trust them under `/hooks`); python3-gi and GStreamer's PipeWire plugin for screen, which asks once which screen to share.

## RGB sync: extend the lighting to other devices
The keyboard streams what its LEDs show to the PC, and `host/rgb_sync.py` paints it onto every device [OpenRGB](https://openrgb.org) can drive (mouse, mouse mat, motherboard, GPU...). Whatever the keyboard shows carries over: every lighting view, brightness, on/off.

- Works in cable mode only; on Bluetooth the other devices keep their last colours.
- The board is sampled as 9 colours, left to right (`rgb_sync.c`). A device zone with several LEDs gets them spread across; a single LED gets the middle of the board.
- The password-layer indicator key is left out, so its PIN-dependent colour never reaches the PC.

Setup on Linux, after flashing: run `host/install.sh [options]`. It installs a udev rule for the keyboard's raw HID interface (sudo) and two systemd user services: an OpenRGB server and `rgb_sync.py`. `host/install.sh --remove` undoes it.

To try the PC side without the keyboard: `openrgb --server &` then `host/rgb_sync.py --demo`. Options for `rgb_sync.py`, given to `install.sh`:
- `--strip-leds N`: set the strip length of addressable (ARGB) headers to N LEDs. Needed for anything plugged into a motherboard ARGB header, e.g. a water block: `host/install.sh --strip-leds 30`.
- `--light ROLE=DEVICE:FIRST[-LAST]`: give some LEDs of a device a role, `cpu`, `memory` or `gpu`. In the system view they show that meter's colour (green to red) instead of following the keyboard; in every other view they follow it as usual. `rgb_sync.py --list` shows the devices and their LED numbers. Example: `--light cpu=B550:3 --light memory=B550:4 --light gpu=B550:5-14`.
- `--skip NAME`: leave devices whose name contains NAME alone.
- `--brightness 0.6`: dim the other devices.

### Lighting from the PC
`host/kbd_light.py` paints colours over the keyboard's own effect: the whole board, nine left-to-right bands, or single LEDs by index. The other devices follow. Painted colours scale with the keyboard's brightness and stay until released, until the optional `--hold` seconds pass, or until the RGB key is pressed (that press only hands the lighting back and does not change the view). Caps lock and the password indicator stay on top.

    host/kbd_light.py color ff8800
    host/kbd_light.py --hold 5 led 0 ff0000
    host/kbd_light.py cpu
    host/kbd_light.py release

It is also a small Python module (`from kbd_light import Keyboard`) for your own programs. This direction needs the `raw_hid_receive_user` hook in `q3_pro.c`.

An ASUS graphics card whose stock cooler was replaced still shows up in OpenRGB, but its lighting chip drives nothing.
