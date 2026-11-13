"""Screen view: the colours along the lower part of the screen, left to right.

Records the screen through the desktop portal (Wayland: KDE, GNOME, ...) with
GStreamer's PipeWire source, scaled down to a thumbnail. The first time, the
desktop asks which screen to share; the choice is remembered, so later starts
are silent. Needs python3-gi and the GStreamer PipeWire plugin.
"""

import os
import time

ZONES = 9
WIDTH, HEIGHT = ZONES * 8, 32  # thumbnail the screen is scaled down to
LOWER_ROWS = 8                 # rows of it, from the bottom, that are used
FRAMES_PER_SECOND = 20
RESEND_SECONDS = 0.2           # a still screen delivers no new frames
PORTAL = "org.freedesktop.portal.Desktop"
PORTAL_PATH = "/org/freedesktop/portal/desktop"
SCREENCAST = "org.freedesktop.portal.ScreenCast"
MONITOR, CURSOR_HIDDEN, PERSIST_UNTIL_REVOKED = 1, 1, 2
ANSWER_SECONDS = 120  # how long the "which screen?" dialog may stay open


class Refused(Exception):
    """The user closed the dialog, or the desktop refused the recording."""


class Cancelled(Exception):
    """The keyboard left the view while the desktop was still asking."""


def token_path():
    config = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(config, "kbd-rgb", "screen-token")


def load_token():
    try:
        with open(token_path()) as f:
            return f.read().strip()
    except OSError:
        return ""


def save_token(token):
    os.makedirs(os.path.dirname(token_path()), mode=0o700, exist_ok=True)
    with open(token_path(), "w") as f:
        f.write(token)


class Portal:
    """A screen cast session: asks for one monitor, hands out its PipeWire stream."""

    def __init__(self, Gio, GLib, wanted):
        self.Gio, self.GLib, self.wanted = Gio, GLib, wanted
        self.context = GLib.MainContext.new()
        self.context.push_thread_default()  # portal answers arrive on this thread
        self.bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        self.sender = self.bus.get_unique_name()[1:].replace(".", "_")
        self.serial = 0
        self.session = None

    def close(self):
        if self.session:
            try:
                self.bus.call_sync(PORTAL, self.session, "org.freedesktop.portal.Session", "Close",
                                   None, None, self.Gio.DBusCallFlags.NONE, 1000, None)
            except self.GLib.Error:
                pass
        self.context.pop_thread_default()

    def _request(self, method, arguments, options, wait_seconds):
        """Calls a portal method and waits for the answer to its request."""
        GLib = self.GLib
        self.serial += 1
        token = f"kbdrgb{os.getpid()}_{self.serial}"
        answer = []
        subscription = self.bus.signal_subscribe(
            PORTAL, "org.freedesktop.portal.Request", "Response",
            f"{PORTAL_PATH}/request/{self.sender}/{token}", None, self.Gio.DBusSignalFlags.NONE,
            lambda *args: answer.append(args[5].unpack()))
        try:
            options = dict(options, handle_token=GLib.Variant("s", token))
            self.bus.call_sync(PORTAL, PORTAL_PATH, SCREENCAST, method,
                               GLib.Variant.new_tuple(*arguments, GLib.Variant("a{sv}", options)),
                               None, self.Gio.DBusCallFlags.NONE, 5000, None)
            deadline = time.monotonic() + wait_seconds
            while not answer:
                if not self.wanted():
                    raise Cancelled()  # closing the session takes the dialog down
                if time.monotonic() > deadline:
                    raise Refused(f"no answer to {method}")
                self.context.iteration(False) or time.sleep(0.05)
        finally:
            self.bus.signal_unsubscribe(subscription)
        code, results = answer[0]
        if code != 0:
            raise Refused(f"{method} was declined")
        return results

    def open(self):
        """Returns (PipeWire fd, node id) of the chosen monitor."""
        GLib = self.GLib
        v = GLib.Variant
        self.session = self._request(
            "CreateSession", [], {"session_handle_token": v("s", f"kbdrgb{os.getpid()}")}, 10)["session_handle"]
        handle = v("o", self.session)

        options = {"types": v("u", MONITOR), "multiple": v("b", False), "cursor_mode": v("u", CURSOR_HIDDEN),
                   "persist_mode": v("u", PERSIST_UNTIL_REVOKED)}
        if load_token():
            options["restore_token"] = v("s", load_token())
        self._request("SelectSources", [handle], options, 10)

        started = self._request("Start", [handle, v("s", "")], {}, ANSWER_SECONDS)
        if started.get("restore_token"):
            save_token(started["restore_token"])
        node = started["streams"][0][0]

        _, fds = self.bus.call_with_unix_fd_list_sync(
            PORTAL, PORTAL_PATH, SCREENCAST, "OpenPipeWireRemote",
            GLib.Variant.new_tuple(handle, v("a{sv}", {})), None, self.Gio.DBusCallFlags.NONE, 5000, None, None)
        return fds.get(0), node


def zone_colors(pixels, stride):
    """Mean colour of each zone over the lower rows of an RGB thumbnail."""
    per_zone = WIDTH // ZONES
    count = per_zone * LOWER_ROWS
    colors = []
    for zone in range(ZONES):
        total = [0, 0, 0]
        for row in range(HEIGHT - LOWER_ROWS, HEIGHT):
            start = row * stride + zone * per_zone * 3
            chunk = pixels[start:start + per_zone * 3]
            for c in range(3):
                total[c] += sum(chunk[c::3])
        colors.append(tuple(t / count for t in total))
    return colors


def run(link, wanted):
    import gi
    gi.require_version("Gst", "1.0")
    from gi.repository import Gio, GLib, Gst
    from views import log

    Gst.init(None)
    portal = Portal(Gio, GLib, wanted)
    pipeline = None
    fd = None
    try:
        try:
            fd, node = portal.open()
        except Cancelled:
            return
        except Refused as e:
            # Asking again at once would only put the dialog back up: wait
            # until the keyboard leaves this view.
            log(f"screen: {e}; select the view again to retry")
            while wanted():
                time.sleep(0.5)
            return
        pipeline = Gst.parse_launch(
            f"pipewiresrc fd={fd} path={node} ! videorate drop-only=true max-rate={FRAMES_PER_SECOND} ! "
            f"videoconvert ! videoscale ! video/x-raw,format=RGB,width={WIDTH},height={HEIGHT} ! "
            "appsink name=sink max-buffers=1 drop=true sync=false")
        sink = pipeline.get_by_name("sink")
        pipeline.set_state(Gst.State.PLAYING)
        colors = [(0, 0, 0)] * ZONES
        while wanted():
            sample = sink.emit("try-pull-sample", int(RESEND_SECONDS * Gst.SECOND))
            if sample:
                buffer = sample.get_buffer()
                ok, mapped = buffer.map(Gst.MapFlags.READ)
                if ok:
                    try:
                        colors = zone_colors(bytes(mapped.data), len(mapped.data) // HEIGHT)
                    finally:
                        buffer.unmap(mapped)
            elif pipeline.get_bus().pop_filtered(Gst.MessageType.ERROR | Gst.MessageType.EOS):
                raise OSError("the screen recording stopped")
            link.canvas(colors)
    finally:
        if pipeline:
            pipeline.set_state(Gst.State.NULL)
        if fd is not None:
            os.close(fd)  # ours: the pipeline works on a copy
        portal.close()
