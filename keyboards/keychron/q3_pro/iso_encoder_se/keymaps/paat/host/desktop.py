"""Shows the keyboard's lighting view on the desktop: an icon in the system
tray (StatusNotifierItem: KDE Plasma, and others with a tray that speaks it)
and a short notification whenever the view changes.

Optional: without python3-gi or a session bus, rgb_sync.py runs without it.
"""

import os
import threading

# view number (rgb_sync.c) -> (name, what it shows, icon from the icon theme)
VIEWS = {
    0: ("Static", "Layout colours", "input-keyboard"),
    1: ("Music", "Spectrum and beat of what is playing", "applications-multimedia"),
    2: ("System", "CPU, memory and GPU load", "utilities-system-monitor"),
    3: ("Coding", "What Claude and Codex are doing", "utilities-terminal"),
    4: ("Screen", "Colours of the lower part of the screen", "video-display"),
}
TITLE = "Keyboard lighting"
TOAST_MS = 2500

ITEM_PATH = "/StatusNotifierItem"
ITEM_XML = """
<node>
  <interface name="org.kde.StatusNotifierItem">
    <property name="Category" type="s" access="read"/>
    <property name="Id" type="s" access="read"/>
    <property name="Title" type="s" access="read"/>
    <property name="Status" type="s" access="read"/>
    <property name="IconName" type="s" access="read"/>
    <property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
    <property name="ItemIsMenu" type="b" access="read"/>
    <method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
    <method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
    <signal name="NewIcon"/>
    <signal name="NewToolTip"/>
    <signal name="NewStatus"><arg type="s"/></signal>
  </interface>
</node>
"""


class Desktop:
    """`show(view)` from any thread; `show(None)` when the keyboard is gone."""

    def __init__(self, log):
        self._log = log
        self._view = None       # what the tray shows
        self._announced = False  # the first view after start is not news
        self._toast_id = 0
        self._context = None
        self._ready = threading.Event()
        threading.Thread(target=self._run, daemon=True).start()

    def show(self, view):
        if view == self._view or not self._ready.wait(0) or self._context is None:
            return
        self._view = view  # set here so that a burst of frames queues one update
        self._context.invoke_full(0, self._update, view)

    # ------------------------------------------------ everything below runs
    # on the desktop thread

    def _run(self):
        try:
            import gi
            from gi.repository import Gio, GLib
            self._Gio, self._GLib = Gio, GLib
            context = GLib.MainContext.new()
            context.push_thread_default()
            self._bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
            interface = Gio.DBusNodeInfo.new_for_xml(ITEM_XML).interfaces[0]
            self._bus.register_object(ITEM_PATH, interface, self._call, self._get, None)
            name = f"org.kde.StatusNotifierItem-{os.getpid()}-1"
            Gio.bus_own_name_on_connection(self._bus, name, Gio.BusNameOwnerFlags.NONE, None, None)
            # (Re-)register whenever a tray appears, e.g. after plasmashell restarts.
            Gio.bus_watch_name_on_connection(
                self._bus, "org.kde.StatusNotifierWatcher", Gio.BusNameWatcherFlags.NONE,
                lambda *_: self._bus.call(
                    "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher",
                    "RegisterStatusNotifierItem", GLib.Variant("(s)", (name,)), None,
                    Gio.DBusCallFlags.NONE, -1, None, None),
                None)
            self._context = context
        except Exception as e:  # no gi, no session bus: do without
            self._log(f"desktop: not available ({type(e).__name__}: {e})")
            self._ready.set()
            return
        self._ready.set()
        GLib.MainLoop.new(context, False).run()

    def _info(self):
        return VIEWS.get(self._view, (f"View {self._view}", "", "input-keyboard"))

    def _get(self, connection, sender, path, interface, prop):
        v = self._GLib.Variant
        name, what, icon = self._info()
        return {
            "Category": v("s", "Hardware"),
            "Id": v("s", "keyboard-rgb"),
            "Title": v("s", TITLE),
            # Passive = hidden: there is nothing to show without the keyboard
            "Status": v("s", "Passive" if self._view is None else "Active"),
            "IconName": v("s", icon),
            "ToolTip": v("(sa(iiay)ss)", (icon, [], f"{TITLE}: {name}", what)),
            "ItemIsMenu": v("b", False),
        }.get(prop)

    def _call(self, connection, sender, path, interface, method, parameters, invocation):
        if method == "Activate" and self._view is not None:
            self._toast()  # a click says what the icon means
        invocation.return_value(None)

    def _emit(self, signal, parameters=None):
        self._bus.emit_signal(None, ITEM_PATH, "org.kde.StatusNotifierItem", signal, parameters)

    def _update(self, view):
        self._emit("NewIcon")
        self._emit("NewToolTip")
        self._emit("NewStatus", self._GLib.Variant("(s)", ("Passive" if view is None else "Active",)))
        if view is not None:
            if self._announced:
                self._toast()
            self._announced = True
        return False  # run once

    def _toast(self):
        GLib, v = self._GLib, self._GLib.Variant
        name, what, icon = self._info()

        def sent(bus, result):
            try:
                self._toast_id = bus.call_finish(result).unpack()[0]
            except GLib.Error:
                pass

        # Replaces the previous toast, and stays out of the notification history.
        self._bus.call(
            "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
            "Notify",
            GLib.Variant("(susssasa{sv}i)", (TITLE, self._toast_id, icon, f"{TITLE}: {name}", what, [],
                                             {"transient": v("b", True)}, TOAST_MS)),
            None, self._Gio.DBusCallFlags.NONE, -1, None, sent)
