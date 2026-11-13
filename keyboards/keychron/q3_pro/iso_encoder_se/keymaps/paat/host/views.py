"""Lighting the PC generates for the keyboard's "views".

The RGB key on the keyboard steps through its lighting views. For those the
PC generates, the keyboard says in every frame which one it is showing
(rgb_sync.c) and rgb_sync.py runs the matching generator from here. A
generator sends the keyboard bars or a canvas of nine zones, left to right,
at least a few times a second; the keyboard draws them and everything else
follows the keyboard.
"""

import os
import sys
import threading
import time

ZONES = 9
REPORT_SIZE = 32
MAGIC = 0xC5
BARS, CANVAS = 0x12, 0x13
RAINBOW, METER = 0, 1  # bar styles
RETRY_SECONDS = 3

# Brightness (0..1) per zone that a running generator wants the other devices
# to show the keyboard's colours at, instead of the keyboard's own brightness.
# The music view sets it so that they pulse with the bass beat. None = as on the
# keyboard.
modulation = None


# Colours a running generator wants particular lights to show instead of the
# keyboard's: {"cpu": (r, g, b), ...}. Which light has which role is the
# user's choice (rgb_sync.py --light). The system view sets it so that the
# light on the CPU shows the CPU load, and so on. None = as on the keyboard.
accents = None


_remembered = []  # per zone: the last colour the keyboard showed there, at full brightness


def modulate(frame):
    """Applies `modulation` to a frame of (r, g, b) zones from the keyboard.

    A zone the keyboard shows dark keeps the colour it had last, so that the
    devices can light up where the keyboard does not (a beat without treble).
    """
    global _remembered
    levels = modulation
    if levels is None or len(levels) != len(frame):
        _remembered = []
        return frame
    if len(_remembered) != len(frame):
        _remembered = [None] * len(frame)
    out = []
    for zone, (color, level) in enumerate(zip(frame, levels)):
        peak = max(color)
        if peak >= 8:
            _remembered[zone] = tuple(v * 255 / peak for v in color)
        full = _remembered[zone]
        out.append(tuple(round(v * level) for v in full) if full else color)
    return out


def log(*args):
    print(*args, file=sys.stderr, flush=True)


class Link:
    """Sends view data to the keyboard's raw HID interface."""

    def __init__(self, path):
        self.fd = os.open(path, os.O_WRONLY)

    def close(self):
        os.close(self.fd)

    def _send(self, *payload):
        report = bytes([MAGIC, *payload]).ljust(REPORT_SIZE, b"\0")
        os.write(self.fd, b"\0" + report)  # leading 0: the report has no ID

    def bars(self, heights, style=RAINBOW):
        """Nine bar heights, 0..1."""
        self._send(BARS, style, *(round(255 * min(1, max(0, h))) for h in heights))

    def canvas(self, colors):
        """Nine (r, g, b) colours, 0..255."""
        self._send(CANVAS, *(min(255, max(0, round(v))) for c in colors for v in c))


class Wanted:
    """Which view the keyboard is showing and where it is: (view, path)."""

    NOTHING = (0, None)

    def __init__(self):
        self._cond = threading.Condition()
        self._value = self.NOTHING

    def set(self, view, path):
        with self._cond:
            if self._value != (view, path):
                self._value = (view, path)
                self._cond.notify_all()

    def get(self):
        with self._cond:
            return self._value

    def wait_while(self, value):
        with self._cond:
            self._cond.wait_for(lambda: self._value != value)


def serve(wanted):
    """Runs the generator for whatever view is wanted. Never returns."""
    # Imported here so that one view's missing dependency does not stop the rest.
    import coding, music, screen, system
    generators = {1: music, 2: system, 3: coding, 4: screen}
    while True:
        want = wanted.get()
        view, path = want
        generator = generators.get(view)
        if generator is None or path is None:
            wanted.wait_while(want)
            continue
        name = generator.__name__
        log(f"{name}: started")
        try:
            link = Link(path)
            try:
                generator.run(link, lambda: wanted.get() == want)
            finally:
                link.close()
            log(f"{name}: stopped")
        except Exception as e:  # a broken view must not take the daemon down
            log(f"{name}: {type(e).__name__}: {e}")
            time.sleep(RETRY_SECONDS)
