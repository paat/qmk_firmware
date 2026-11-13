#!/bin/sh
# Sets up the PC side of RGB sync (see the keymap readme):
#  - a udev rule so you can read the keyboard's raw HID interface (needs sudo)
#  - two systemd user services, started at login: an OpenRGB server and
#    rgb_sync.py
# Arguments are passed on to rgb_sync.py, e.g.: install.sh --strip-leds 30
# Undo with: install.sh --remove
set -e
here=$(cd "$(dirname "$0")" && pwd)
rule=/etc/udev/rules.d/70-keychron-rgb-sync.rules
units=${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user

if [ "$1" = "--remove" ]; then
    systemctl --user disable --now keyboard-rgb-sync.service openrgb-server.service || true
    rm -f "$units/keyboard-rgb-sync.service" "$units/openrgb-server.service"
    systemctl --user daemon-reload
    sudo rm -f "$rule"
    sudo udevadm control --reload
    exit 0
fi

sudo install -m 644 "$here/70-keychron-rgb-sync.rules" "$rule"
sudo udevadm control --reload
sudo udevadm trigger --subsystem-match=hidraw --action=add

mkdir -p "$units"
cat > "$units/openrgb-server.service" <<EOF
[Unit]
Description=OpenRGB SDK server

[Service]
ExecStart=$(command -v openrgb) --server
Restart=on-failure

[Install]
WantedBy=default.target
EOF
cat > "$units/keyboard-rgb-sync.service" <<EOF
[Unit]
Description=Extend the keyboard's RGB lighting to other devices
Wants=openrgb-server.service
After=openrgb-server.service

[Service]
ExecStart=/usr/bin/env python3 $here/rgb_sync.py $*
Restart=on-failure

[Install]
WantedBy=default.target
EOF
systemctl --user daemon-reload
systemctl --user enable openrgb-server.service keyboard-rgb-sync.service
systemctl --user restart openrgb-server.service keyboard-rgb-sync.service
echo "Installed. Follow it with: journalctl --user -fu keyboard-rgb-sync"
