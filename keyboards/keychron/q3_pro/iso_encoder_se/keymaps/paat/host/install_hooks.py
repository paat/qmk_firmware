#!/usr/bin/env python3
"""Adds (or with --remove, removes) the hooks that report what Claude Code
and Codex are doing, for the keyboard's coding view. Touches only its own
entries in ~/.claude/settings.json and ~/.codex/hooks.json.

Codex runs a new hook only after you have trusted it: open /hooks in Codex.
"""

import json
import os
import sys

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "agent_state.py")

# event -> (state, matcher)
CLAUDE = {
    "UserPromptSubmit": ("working", None),
    "PostToolUse": ("working", None),
    "PermissionRequest": ("waiting", None),
    "Notification": ("waiting", "permission_prompt|elicitation_dialog"),
    "Stop": ("done", None),
    "SessionEnd": ("end", None),
}
CODEX = {
    "UserPromptSubmit": ("working", None),
    "PostToolUse": ("working", None),
    "PermissionRequest": ("waiting", None),
    "Stop": ("done", None),
    "SessionEnd": ("end", None),
}


def is_ours(group):
    return any("agent_state.py" in hook.get("command", "") for hook in group.get("hooks", []))


def update(path, agent, events, remove):
    try:
        with open(path) as f:
            settings = json.load(f)
    except FileNotFoundError:
        settings = {}
    hooks = settings.setdefault("hooks", {})
    for event in list(hooks):
        hooks[event] = [group for group in hooks[event] if not is_ours(group)]
        if not hooks[event]:
            del hooks[event]
    if not remove:
        for event, (state, matcher) in events.items():
            if agent == "claude":  # exec form: no shell involved
                hook = {"type": "command", "command": SCRIPT, "args": [agent, state], "async": True, "timeout": 5}
            else:
                hook = {"type": "command", "command": f"'{SCRIPT}' {agent} {state}", "async": True, "timeout": 5}
            if event == "SessionEnd" and agent == "codex":
                hook["timeout"] = 1
            group = {"hooks": [hook]}
            if matcher:
                group["matcher"] = matcher
            hooks.setdefault(event, []).append(group)
    if not hooks:
        del settings["hooks"]
    if not settings and not os.path.exists(path):
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + ".tmp", "w") as f:
        json.dump(settings, f, indent=2)
        f.write("\n")
    os.replace(path + ".tmp", path)
    print(("removed from" if remove else "installed in"), path)


def main():
    remove = "--remove" in sys.argv[1:]
    home = os.path.expanduser("~")
    update(os.path.join(home, ".claude", "settings.json"), "claude", CLAUDE, remove)
    update(os.path.join(home, ".codex", "hooks.json"), "codex", CODEX, remove)
    if not remove:
        print("Codex: open /hooks once to trust the new hooks.")


if __name__ == "__main__":
    main()
