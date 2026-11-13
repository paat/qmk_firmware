#!/usr/bin/env python3
"""Paint the keyboard's lighting from the PC (paat keymap, cable mode).

Painted colours lie over the keyboard's own effect until released; the other
RGB devices follow through rgb_sync.py. The RGB key on the keyboard releases
them too. Colours are RRGGBB hex.

    kbd_light.py color ff8800            whole board
    kbd_light.py zones RRGGBB x9         nine bands, left to right
    kbd_light.py led 0 ff0000 5 00ff00   single LEDs, by index
    kbd_light.py cpu                     live CPU load bar, until Ctrl-C
    kbd_light.py release                 back to the keyboard's own effect

As a module:

    from kbd_light import Keyboard
    kb = Keyboard()
    kb.fill((255, 0, 0), hold=5)   # red; reverts by itself after 5 s
"""

import argparse
import os
import sys
import time

from rgb_sync import find_keyboard

ZONES = 9
LED_COUNT = 92
REPORT_SIZE = 32
MAGIC = 0xC5
SET_ZONES, SET_LEDS, RELEASE = 0x10, 0x11, 0x1F
LEDS_PER_REPORT = 9


class Keyboard:
    """`hold`: seconds a paint stays without being repeated; 0 = until released."""

    def __init__(self):
        path = find_keyboard()
        if path is None:
            raise OSError("keyboard not found on USB")
        self.fd = os.open(path, os.O_WRONLY)

    def _send(self, *payload):
        report = bytes([MAGIC, *payload]).ljust(REPORT_SIZE, b"\0")
        os.write(self.fd, b"\0" + report)  # leading 0: the report has no ID

    def zones(self, colors, hold=0):
        """Nine (r, g, b) colours, left to right."""
        if len(colors) != ZONES:
            raise ValueError(f"need {ZONES} colours")
        self._send(SET_ZONES, hold, *(v for c in colors for v in c))

    def fill(self, color, hold=0):
        self.zones([color] * ZONES, hold)

    def leds(self, start, colors, hold=0):
        """Paint LEDs start, start+1, ...; the rest keep what they show."""
        for i in range(0, len(colors), LEDS_PER_REPORT):
            chunk = colors[i:i + LEDS_PER_REPORT]
            self._send(SET_LEDS, hold, start + i, len(chunk), *(v for c in chunk for v in c))

    def release(self):
        self._send(RELEASE)


def color(text):
    text = text.lstrip("#")
    if len(text) != 6:
        raise argparse.ArgumentTypeError(f"not an RRGGBB colour: {text}")
    try:
        return tuple(bytes.fromhex(text))
    except ValueError:
        raise argparse.ArgumentTypeError(f"not an RRGGBB colour: {text}")


def cpu_times():
    with open("/proc/stat") as f:
        fields = [int(v) for v in f.readline().split()[1:]]
    return sum(fields), fields[3] + fields[4]  # total, idle + iowait


def cpu_bar(kb):
    """A bar that grows left to right and turns from green to red with load."""
    total, idle = cpu_times()
    load = 0.0
    try:
        while True:
            time.sleep(0.25)
            new_total, new_idle = cpu_times()
            if new_total > total:
                now = 1 - (new_idle - idle) / (new_total - total)
                load += (now - load) * 0.5
            total, idle = new_total, new_idle
            lit = (int(255 * min(1, 2 * load)), int(255 * min(1, 2 - 2 * load)), 0)
            bar = load * ZONES
            kb.zones([tuple(int(v * max(0.04, min(1, bar - z))) for v in lit) for z in range(ZONES)], hold=2)
    finally:
        kb.release()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--hold", type=int, default=0, metavar="S",
                        help="revert after S seconds (1-255; default: stay until released)")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("color").add_argument("rgb", type=color)
    commands.add_parser("zones").add_argument("rgb", type=color, nargs=ZONES)
    commands.add_parser("led").add_argument("pairs", nargs="+", metavar="INDEX RRGGBB")
    commands.add_parser("cpu")
    commands.add_parser("release")
    args = parser.parse_args()
    if not 0 <= args.hold <= 255:
        parser.error("--hold must be 0-255")

    kb = Keyboard()
    if args.command == "color":
        kb.fill(args.rgb, args.hold)
    elif args.command == "zones":
        kb.zones(args.rgb, args.hold)
    elif args.command == "led":
        if len(args.pairs) % 2:
            parser.error("led takes INDEX RRGGBB pairs")
        for index, rgb in zip(args.pairs[::2], args.pairs[1::2]):
            if not index.isdigit() or int(index) >= LED_COUNT:
                parser.error(f"LED index must be 0-{LED_COUNT - 1}: {index}")
            try:
                kb.leds(int(index), [color(rgb)], args.hold)
            except argparse.ArgumentTypeError as e:
                parser.error(str(e))
    elif args.command == "cpu":
        cpu_bar(kb)
    elif args.command == "release":
        kb.release()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except OSError as e:
        sys.exit(f"kbd_light: {e}")
