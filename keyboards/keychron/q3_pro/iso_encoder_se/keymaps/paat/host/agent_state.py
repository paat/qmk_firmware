#!/usr/bin/env python3
"""Hook for Claude Code and Codex: records what an agent session is doing.

    agent_state.py claude|codex working|waiting|done|end

The hook's JSON arrives on stdin; only its session_id is used. The state goes
into a small file per session that the coding view (coding.py) reads. Prints
nothing and always succeeds, so it can never disturb the agent.
"""

import json
import os
import re
import sys

STATES = ("working", "waiting", "done")


def state_dir():
    return os.path.join(os.environ.get("XDG_RUNTIME_DIR") or "/tmp", "kbd-rgb", "agents")


def main():
    agent, state = sys.argv[1:3]
    try:
        session = str(json.load(sys.stdin).get("session_id", ""))
    except (ValueError, AttributeError):
        session = ""
    name = f"{agent}-{re.sub(r'[^A-Za-z0-9_-]', '', session)[:64] or 'unknown'}"
    path = os.path.join(state_dir(), name)
    if state == "end":
        os.unlink(path)
    elif state in STATES and agent in ("claude", "codex"):
        os.makedirs(state_dir(), mode=0o700, exist_ok=True)
        with open(path + ".tmp", "w") as f:
            f.write(state)
        os.replace(path + ".tmp", path)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
