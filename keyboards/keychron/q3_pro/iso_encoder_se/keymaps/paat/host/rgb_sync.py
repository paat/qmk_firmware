#!/usr/bin/env python3
"""Extend the keyboard's RGB lighting onto the PC's other RGB devices.

The paat keymap streams what its LEDs show (rgb_sync.c): a few colours, left
to right across the board. This reads them from the keyboard's raw HID
interface and paints them onto every device of a running OpenRGB server
(`openrgb --server`). Standard library only.

    rgb_sync.py            mirror the keyboard
    rgb_sync.py --demo     moving rainbow instead, to test the PC side
    rgb_sync.py --list     show the OpenRGB devices and exit
"""

import argparse
import colorsys
import glob
import os
import select
import socket
import struct
import sys
import threading
import time

import views
from desktop import Desktop

KEYBOARD_HID_ID = "0003:00003434:00000631"  # USB, Keychron, Q3 Pro
RAW_HID_DESCRIPTOR = bytes.fromhex("0660ff0961")  # usage page 0xFF60, usage 0x61
FRAME_MAGIC = bytes([0xC5, 0x01])
FRAME_FLAGS = 30  # byte of the frame holding the flags
FLAGS_VIEW = 0x0F  # the PC-generated view the keyboard shows; 0 = none
RETRY_SECONDS = 2
REFRESH_SECONDS = 5  # repaint even unchanged colours: other tools may overwrite them


def log(*args):
    print(*args, file=sys.stderr, flush=True)


# ---------------------------------------------------------------- frames

class Latest:
    """The newest frame (a list of (r, g, b) zones), handed between threads."""

    def __init__(self):
        self._cond = threading.Condition()
        self._frame = None
        self._serial = 0

    def put(self, frame):
        with self._cond:
            self._frame = frame
            self._serial += 1
            self._cond.notify_all()

    def wait_newer(self, serial, timeout):
        with self._cond:
            self._cond.wait_for(lambda: self._serial != serial, timeout)
            return self._frame, self._serial


def find_keyboard():
    for sysfs in sorted(glob.glob("/sys/class/hidraw/hidraw*")):
        try:
            with open(sysfs + "/device/uevent") as f:
                if f"HID_ID={KEYBOARD_HID_ID}" not in f.read().upper():
                    continue
            with open(sysfs + "/device/report_descriptor", "rb") as f:
                if f.read(len(RAW_HID_DESCRIPTOR)) == RAW_HID_DESCRIPTOR:
                    return "/dev/" + os.path.basename(sysfs)
        except OSError:
            continue
    return None


def parse_frame(report):
    if len(report) < 3 or report[:2] != FRAME_MAGIC:
        return None  # some other raw HID traffic
    zones = report[2]
    data = report[3:3 + zones * 3]
    if zones == 0 or len(data) != zones * 3:
        return None
    return [tuple(data[i:i + 3]) for i in range(0, len(data), 3)]


def wanted_view(report):
    return report[FRAME_FLAGS] & FLAGS_VIEW if len(report) > FRAME_FLAGS else 0


def read_keyboard(latest, wanted):
    desktop = Desktop(log)  # tray icon and notifications for the current view
    complained = None
    while True:
        path = find_keyboard()
        try:
            if path is None:
                raise OSError("keyboard not found on USB")
            fd = os.open(path, os.O_RDONLY)
        except OSError as e:
            if str(e) != complained:
                complained = str(e)
                log(f"keyboard: {e}; waiting")
                if isinstance(e, PermissionError):
                    log("  install the udev rule: see readme, 'RGB sync'")
            time.sleep(RETRY_SECONDS)
            continue
        complained = None
        log(f"keyboard: reading {path}")
        silent_since = time.monotonic()
        try:
            while True:
                if select.select([fd], [], [], 5)[0]:
                    report = os.read(fd, 64)
                    frame = parse_frame(report)
                    if frame:
                        latest.put(frame)
                        wanted.set(wanted_view(report), path)
                        desktop.show(wanted_view(report))
                        silent_since = None
                else:
                    wanted.set(*wanted.NOTHING)  # no frames: Bluetooth, or asleep
                    desktop.show(None)
                    if silent_since and time.monotonic() - silent_since > 5:
                        log("keyboard: no frames; is the rgb_sync firmware flashed "
                            "and the board in cable mode?")
                        silent_since = None
        except OSError as e:
            log(f"keyboard: {e}")
        finally:
            wanted.set(*wanted.NOTHING)
            desktop.show(None)
            os.close(fd)
        time.sleep(RETRY_SECONDS)


def demo(latest, zones=9):
    start = time.monotonic()
    while True:
        t = time.monotonic() - start
        latest.put([tuple(int(c * 255) for c in colorsys.hsv_to_rgb((z / zones - t / 4) % 1, 1, 1))
                    for z in range(zones)])
        time.sleep(0.04)


# --------------------------------------------------------------- OpenRGB

class OpenRGB:
    """Just enough of the OpenRGB SDK protocol (version 0) to set colours."""

    REQUEST_CONTROLLER_COUNT = 0
    REQUEST_CONTROLLER_DATA = 1
    SET_CLIENT_NAME = 50
    RESIZE_ZONE = 1000
    DEVICE_LIST_UPDATED = 100
    UPDATE_LEDS = 1050
    SET_CUSTOM_MODE = 1100
    UPDATE_MODE = 1101

    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.devices_changed = False
        self._send(0, self.SET_CLIENT_NAME, b"keyboard rgb_sync\0")

    def close(self):
        self.sock.close()

    def _send(self, device, packet, data=b""):
        self.sock.sendall(struct.pack("<4sIII", b"ORGB", device, packet, len(data)) + data)

    def _recv_exact(self, size):
        buf = b""
        while len(buf) < size:
            # The server sends a reply in two parts; without this the kernel
            # delays the ACK between them and every reply takes 40 ms.
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_QUICKACK, 1)
            chunk = self.sock.recv(size - len(buf))
            if not chunk:
                raise ConnectionError("OpenRGB server closed the connection")
            buf += chunk
        return buf

    def _request(self, device, packet, data=b""):
        self._send(device, packet, data)
        while True:
            magic, _, got, size = struct.unpack("<4sIII", self._recv_exact(16))
            if magic != b"ORGB":
                raise ConnectionError("bad reply from OpenRGB server")
            body = self._recv_exact(size)
            if got == packet:
                return body
            if got == self.DEVICE_LIST_UPDATED:
                self.devices_changed = True

    def sync(self):
        """Round trip to the server; also picks up its notifications."""
        self._request(0, self.REQUEST_CONTROLLER_COUNT)

    def controllers(self):
        """[(index, name, [LED count per zone], direct mode or None, [resizable zones])]"""
        count = struct.unpack("<I", self._request(0, self.REQUEST_CONTROLLER_COUNT))[0]
        return [(i, *self._parse_controller(self._request(i, self.REQUEST_CONTROLLER_DATA)))
                for i in range(count)]

    @staticmethod
    def _parse_controller(data):
        pos = 0

        def take(fmt):
            nonlocal pos
            values = struct.unpack_from("<" + fmt, data, pos)
            pos += struct.calcsize("<" + fmt)
            return values[0]

        def text():
            nonlocal pos
            length = take("H")
            raw = data[pos:pos + length]
            pos += length
            return raw.rstrip(b"\0").decode(errors="replace")

        take("I")  # data size
        take("i")  # device type
        name = text()
        for _ in range(4):  # description, version, serial, location
            text()
        modes = take("H")
        take("i")  # active mode
        direct = None
        for mode in range(modes):
            start = pos
            mode_name = text()
            pos += 9 * 4  # value, flags, speed min/max, colours min/max, speed, direction, colour mode
            colors = take("H")
            pos += colors * 4
            if mode_name == "Direct":
                direct = struct.pack("<i", mode) + data[start:pos]
        zones = []
        resizable = []  # zones for LED strips, whose length is up to the user
        for zone in range(take("H")):
            text()
            take("i")  # type
            leds_min = take("I")
            leds_max = take("I")
            zones.append(take("I"))
            if leds_max > leds_min:
                resizable.append(zone)
            matrix = take("H")
            pos += matrix
        leds = take("H")
        if sum(zones) != leds:
            zones = [leds]
        return name, zones, direct, resizable

    def resize_zone(self, device, zone, leds):
        self._send(device, self.RESIZE_ZONE, struct.pack("<ii", zone, leds))

    def take_control(self, device, direct):
        """Put the device in the mode where it shows the colours it is sent."""
        if direct:
            # Selecting Direct explicitly: SET_CUSTOM_MODE alone leaves some
            # devices (ASUS Aura GPUs) running their previous effect.
            self._send(device, self.UPDATE_MODE, struct.pack("<I", len(direct) + 4) + direct)
        else:
            self._send(device, self.SET_CUSTOM_MODE)

    def set_leds(self, device, colors):
        body = struct.pack("<H", len(colors)) + b"".join(bytes((r, g, b, 0)) for r, g, b in colors)
        self._send(device, self.UPDATE_LEDS, struct.pack("<I", len(body) + 4) + body)


# --------------------------------------------------------------- mapping

def spread(frame, leds):
    """Stretch the keyboard's zones over `leds` LEDs; one LED gets the middle."""
    return [frame[(2 * i + 1) * len(frame) // (2 * leds)] for i in range(leds)]


def colors_for(frame, zones, brightness):
    colors = [c for leds in zones for c in spread(frame, leds)]
    if brightness != 1:
        colors = [tuple(min(255, round(v * brightness)) for v in c) for c in colors]
    return colors


def light(text):
    """--light ROLE=DEVICE:FIRST[-LAST] -> (role, device, first, last)"""
    try:
        role, where = text.split("=", 1)
        device, leds = where.rsplit(":", 1)
        first, _, last = leds.partition("-")
        first, last = int(first), int(last or first)
        if not role or not device or not 0 <= first <= last:
            raise ValueError
    except ValueError:
        raise argparse.ArgumentTypeError(f"not ROLE=DEVICE:FIRST[-LAST]: {text}")
    return role, device.lower(), first, last


ACCENT_FULL = 150  # brightest channel of a frame at which accents are at full brightness


def with_accents(colors, name, lights, frame, brightness):
    """Lights with a role show the colour a view has set for that role.

    They dim along with the keyboard: its brightness is not reported, but how
    bright the frame is as a whole follows it, down to off.
    """
    accents = views.accents
    if not accents:
        return colors
    brightness *= min(1, max(max(c) for c in frame) / ACCENT_FULL)
    for role, device, first, last in lights:
        if role in accents and device in name.lower():
            accent = tuple(min(255, round(v * brightness)) for v in accents[role])
            colors = [accent if first <= i <= last else c for i, c in enumerate(colors)]
    return colors


def mirror(latest, args):
    rgb = OpenRGB(args.host, args.port)
    try:
        if args.strip_leds:
            for index, _, zones, _, resizable in rgb.controllers():
                for zone in resizable:
                    if zones[zone] != args.strip_leds:
                        rgb.resize_zone(index, zone, args.strip_leds)
        devices = [d for d in rgb.controllers()
                   if sum(d[2]) and not any(s.lower() in d[1].lower() for s in args.skip)]
        if not devices:
            raise ConnectionError("OpenRGB server reports no devices with LEDs")
        for index, name, zones, direct, _ in devices:
            log(f"openrgb: {name}: {sum(zones)} LEDs")
            rgb.take_control(index, direct)
        shown = {}
        refreshed = time.monotonic()
        frame, serial = latest.wait_newer(0, 0)
        while not rgb.devices_changed:
            if time.monotonic() - refreshed > REFRESH_SECONDS:
                shown.clear()
                refreshed = time.monotonic()
            if frame:
                shown_frame = views.modulate(frame)
                for index, name, zones, _, _ in devices:
                    colors = with_accents(colors_for(shown_frame, zones, args.brightness), name, args.light,
                                          frame, args.brightness)
                    if shown.get(index) != colors:
                        rgb.set_leds(index, colors)
                        shown[index] = colors
            rgb.sync()  # notices a dead server or a changed device list
            frame, serial = latest.wait_newer(serial, REFRESH_SECONDS)
        log("openrgb: device list changed")
    finally:
        rgb.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1", help="OpenRGB server (default %(default)s)")
    parser.add_argument("--port", type=int, default=6742)
    parser.add_argument("--skip", action="append", default=[], metavar="NAME",
                        help="leave devices whose name contains NAME alone (repeatable)")
    parser.add_argument("--brightness", type=float, default=1.0, help="scale the colours, e.g. 0.6")
    parser.add_argument("--strip-leds", type=int, default=0, metavar="N",
                        help="set the strip length of every addressable header to N LEDs")
    parser.add_argument("--light", action="append", default=[], type=light, metavar="ROLE=DEVICE:FIRST[-LAST]",
                        help="give LEDs FIRST..LAST of the device whose name contains DEVICE a role (cpu, memory, "
                             "gpu) that views can colour by themselves, e.g. cpu=B550:3 (repeatable)")
    parser.add_argument("--demo", action="store_true", help="show a moving rainbow instead of the keyboard")
    parser.add_argument("--list", action="store_true", help="list OpenRGB devices and exit")
    args = parser.parse_args()

    if args.list:
        rgb = OpenRGB(args.host, args.port)
        for index, name, zones, _, _ in rgb.controllers():
            print(f"{index}: {name}: zones of {zones} LEDs (LED numbers 0-{sum(zones) - 1})")
        return

    latest = Latest()
    if args.demo:
        threading.Thread(target=demo, args=(latest,), daemon=True).start()
    else:
        wanted = views.Wanted()
        threading.Thread(target=read_keyboard, args=(latest, wanted), daemon=True).start()
        threading.Thread(target=views.serve, args=(wanted,), daemon=True).start()

    complained = None
    while True:
        try:
            mirror(latest, args)
            complained = None
        except (OSError, struct.error) as e:
            if str(e) != complained:
                complained = str(e)
                log(f"openrgb: {e}; retrying")
        time.sleep(RETRY_SECONDS)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
