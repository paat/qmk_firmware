"""System view: three meters, left to right: CPU load, memory in use, GPU load.

Each fills from the bottom and turns from green to red as it rises. Lights
given the role "cpu", "memory" or "gpu" (rgb_sync.py --light) show that
meter's colour by themselves.
"""

import colorsys
import glob
import time

import views
from views import METER

INTERVAL = 0.1   # seconds between updates
SMOOTHING = 0.25  # share of each new reading taken per update
FLOOR = 0.06     # so that an idle meter still shows its bottom row
# For the lights with a role: the reading at which each is still fully green,
# and the one at which it is fully red. Memory is never near empty, so its
# scale starts higher to keep everyday changes visible.
GREEN_RED = {"cpu": (0.05, 0.85), "memory": (0.5, 0.9), "gpu": (0.05, 0.85)}


def cpu_times():
    with open("/proc/stat") as f:
        fields = [int(v) for v in f.readline().split()[1:]]
    return sum(fields), fields[3] + fields[4]  # total, idle + iowait


def memory_used():
    values = {}
    with open("/proc/meminfo") as f:
        for line in f:
            name, value = line.split(":")
            values[name] = int(value.split()[0])
    return 1 - values["MemAvailable"] / values["MemTotal"]


def gpu_load():
    """Busiest GPU, 0..1; 0 when the driver does not report it (amdgpu does)."""
    loads = []
    for path in glob.glob("/sys/class/drm/card*/device/gpu_busy_percent"):
        try:
            with open(path) as f:
                loads.append(int(f.read()) / 100)
        except (OSError, ValueError):
            pass
    return max(loads, default=0)


def meter_color(reading, green, red):
    """Green up to `green`, through yellow, to red from `red` on."""
    position = min(1, max(0, (reading - green) / (red - green)))
    return tuple(round(255 * v) for v in colorsys.hsv_to_rgb((1 - position) / 3, 1, 1))


def run(link, wanted):
    total, idle = cpu_times()
    shown = [0.0, 0.0, 0.0]
    try:
        while wanted():
            time.sleep(INTERVAL)
            new_total, new_idle = cpu_times()
            cpu = 1 - (new_idle - idle) / (new_total - total) if new_total > total else shown[0]
            total, idle = new_total, new_idle
            for i, reading in enumerate((cpu, memory_used(), gpu_load())):
                shown[i] += (reading - shown[i]) * SMOOTHING
            link.bars([max(FLOOR, v) for v in shown for _ in range(3)], METER)
            views.accents = {role: meter_color(reading, *GREEN_RED[role])
                             for role, reading in zip(("cpu", "memory", "gpu"), shown)}
    finally:
        views.accents = None
