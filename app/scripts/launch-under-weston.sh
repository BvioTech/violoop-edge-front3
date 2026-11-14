#!/bin/bash
# Launches the real app (not a spike) under weston/kiosk-shell, same
# pattern as ../../ui-interface-spike/scripts/launch-under-weston.sh and
# ../../qt3d-spike/scripts/launch-under-weston.sh - see those for why
# /opt/violoop-qt6 (not system apt Qt6) and why this needs the systemd
# unit's PAMName=login + TTYPath=/dev/tty7 seat/KMS path rather than plain
# SSH.
#
# Intended use: swap in as a temporary violoop-edge-kiosk.service
# ExecStart override (systemctl edit), swap back to the Flutter launcher
# afterwards.
set -uo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${EDGE_QT_BIN:-$APP_DIR/build/violoop_edge_kiosk_qt}"

QT_PREFIX="${EDGE_QT_PREFIX:-/opt/violoop-qt6}"
export LD_LIBRARY_PATH="$QT_PREFIX/lib:${LD_LIBRARY_PATH:-}"
export QML2_IMPORT_PATH="$QT_PREFIX/qml:${QML2_IMPORT_PATH:-}"
export QT_PLUGIN_PATH="$QT_PREFIX/plugins:${QT_PLUGIN_PATH:-}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
export QSG_RHI_BACKEND=opengl
export QT_LOGGING_RULES="${QT_LOGGING_RULES:-qt.qpa.*=true;qt.scenegraph.general=true}"

rm -f "$XDG_RUNTIME_DIR/wayland-0" "$XDG_RUNTIME_DIR/wayland-0.lock"

RUN_DIR="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}"
mkdir -p "$RUN_DIR"
# No repaint-window in [core] below, on purpose. The launchers this one
# descends from set repaint-window=-1, which holds every client on this
# 60 Hz panel to 30 fps: weston then starts compositing too late for the
# next refresh, so each frame goes out one refresh behind (measured on
# dev1, 2026-11-13: 28-29 fps with -1 or 0, 46-57 with weston's default of
# 7 ms - see app/README.md, "性能标杆").
WESTON_INI="$RUN_DIR/weston.ini"
cat >"$WESTON_INI" <<INI
[core]
backend=drm-backend.so
require-input=false
idle-time=0
xwayland=false

[shell]
locking=false
panel-position=none

[output]
name=DSI-1
transform=normal
INI

WESTON_LOG="$RUN_DIR/weston.log"
/usr/bin/weston --config="$WESTON_INI" --shell=kiosk-shell.so \
    --socket=wayland-0 --tty=7 --idle-time=0 --debug \
    >"$WESTON_LOG" 2>&1 &
WESTON_PID=$!

for _ in $(seq 1 40); do
    if [ -S "$XDG_RUNTIME_DIR/wayland-0" ] \
       && grep -q "kiosk-shell" "$WESTON_LOG" 2>/dev/null; then
        break
    fi
    sleep 0.25
done
sleep 1

trap 'kill -TERM $WESTON_PID 2>/dev/null' EXIT TERM INT

exec "$BIN" >"$RUN_DIR/app.log" 2>&1
