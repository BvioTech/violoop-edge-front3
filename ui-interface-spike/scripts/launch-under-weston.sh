#!/bin/bash
# Spike launcher: same weston/seat setup as
# ../../qt3d-spike/scripts/launch-under-weston.sh (itself copied from
# violoop-edge-front2's packaging/usr/bin/violoop-edge-kiosk-launch), but
# exec'ing this spike's UI/interface binary instead. See
# ../../qt3d-spike/docs/findings.md for why this links against
# /opt/violoop-qt6 (the officially published, GLES-build-fixed Qt package)
# rather than the plain Debian apt qt6-base-dev: that apt build reproduced
# the same "QSurfaceFormat stuck at version 2.0" bug that broke skinned
# glTF rendering there. This spike's 2D QtQuick content likely wouldn't
# hit that specific bug (it doesn't need GLSL ES 3.00 features), but
# there's no reason to risk it when the known-good prefix is already on
# this device.
#
# Intended use: swap in as a temporary violoop-edge-kiosk.service
# ExecStart override (systemctl edit), same as qt3d-spike - inherits the
# PAMName=login + TTYPath=/dev/tty7 seat/KMS acquisition path. Swap back to
# the Flutter launcher afterwards. Not meant to run standalone over plain
# SSH (weston's drm-backend needs a logind/seatd session for KMS).
#
# Usage: launch-under-weston.sh [scene]
#   scene: one of rotation_touch / voice_state / cjk_text / combined
#   (default combined - see ../main.cpp). Matches validation items A/D/E/F
#   in ../docs/findings.md.
set -uo pipefail

SPIKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${UI_SPIKE_BIN:-$SPIKE_DIR/build/ui_interface_spike}"
SCENE="${1:-combined}"

QT_PREFIX="${UI_SPIKE_QT_PREFIX:-/opt/violoop-qt6}"
export LD_LIBRARY_PATH="$QT_PREFIX/lib:${LD_LIBRARY_PATH:-}"
export QML2_IMPORT_PATH="$QT_PREFIX/qml:${QML2_IMPORT_PATH:-}"
export QT_PLUGIN_PATH="$QT_PREFIX/plugins:${QT_PLUGIN_PATH:-}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
# Default (QStandardPaths::TemporaryLocation, i.e. /tmp) is a shared,
# non-service-owned path - a leftover file there from a manual test run as
# a different user leaves PerfMonitor unable to open/truncate it under
# whatever user this service runs as ("Permission denied", silently no CSV
# written). Use the systemd RuntimeDirectory instead, which is created
# fresh and owned by this service's User= on every start.
export UI_SPIKE_PERF_LOG="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}/perf.csv"
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
export QSG_RHI_BACKEND=opengl
export QT_LOGGING_RULES="qt.qpa.*=true;qt.scenegraph.general=true;qt.rhi.general=true"

rm -f "$XDG_RUNTIME_DIR/wayland-0" "$XDG_RUNTIME_DIR/wayland-0.lock"

RUN_DIR="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}"
mkdir -p "$RUN_DIR"
WESTON_INI="$RUN_DIR/weston.ini"
cat >"$WESTON_INI" <<INI
[core]
backend=drm-backend.so
require-input=false
idle-time=0
repaint-window=-1
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

exec "$BIN" "$SCENE" >"$RUN_DIR/ui-interface-spike.log" 2>&1
