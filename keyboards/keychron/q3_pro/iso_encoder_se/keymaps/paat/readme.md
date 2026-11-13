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
