"""Coding view: what the coding agents are doing. Left half Claude, right Codex.

    working   the agent's colour, with a pulse running across its half
    waiting   blinking yellow: it needs an answer or a permission from you
    done      steady green for a few minutes after it finished
    idle      the agent's colour, dim

The states come from hooks in Claude Code and Codex (agent_state.py,
install_hooks.py). With several sessions of one agent the most urgent wins.
"""

import math
import os
import time

from agent_state import state_dir

FRAME_SECONDS = 1 / 30
RESCAN_SECONDS = 0.25
# A state nobody updated for this long is no longer believed (seconds).
EXPIRY = {"working": 30 * 60, "waiting": 12 * 3600, "done": 5 * 60}
URGENCY = ("idle", "done", "working", "waiting")

COLORS = {"claude": (255, 95, 25), "codex": (40, 130, 255)}
ZONES = {"claude": (0, 1, 2, 3), "codex": (5, 6, 7, 8)}  # zone 4 stays dark
WAITING = (255, 200, 0)
DONE = (0, 255, 0)
IDLE_LEVEL = 0.12


def agent_states():
    states = dict.fromkeys(COLORS, "idle")
    try:
        names = os.listdir(state_dir())
    except OSError:
        return states
    now = time.time()
    for name in names:
        agent = name.split("-", 1)[0]
        path = os.path.join(state_dir(), name)
        try:
            with open(path) as f:
                state = f.read().strip()
            age = now - os.stat(path).st_mtime
        except OSError:
            continue
        if agent in states and state in EXPIRY and age < EXPIRY[state]:
            states[agent] = max(states[agent], state, key=URGENCY.index)
    return states


def scale(color, level):
    return tuple(v * level for v in color)


def paint(states, t):
    canvas = [(0, 0, 0)] * 9
    for agent, zones in ZONES.items():
        state = states[agent]
        for i, zone in enumerate(zones):
            if state == "working":
                wave = 0.5 + 0.5 * math.sin(2 * math.pi * (t / 1.2 - i / len(zones)))
                canvas[zone] = scale(COLORS[agent], 0.25 + 0.75 * wave)
            elif state == "waiting":
                canvas[zone] = scale(WAITING, 1.0 if t % 0.6 < 0.3 else 0.08)
            elif state == "done":
                canvas[zone] = DONE
            else:
                canvas[zone] = scale(COLORS[agent], IDLE_LEVEL)
    return canvas


def run(link, wanted):
    states, scanned = agent_states(), time.monotonic()
    while wanted():
        now = time.monotonic()
        if now - scanned > RESCAN_SECONDS:
            states, scanned = agent_states(), now
        link.canvas(paint(states, now))
        time.sleep(FRAME_SECONDS)
