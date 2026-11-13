#!/bin/bash
# Spike launcher: reuse the exact weston/seat setup of
# violoop-edge-front2's packaging/usr/bin/violoop-edge-kiosk-launch, but
# exec this repo's Qt Quick 3D test binary instead of Flutter, to verify
# EGL/GLES hardware acceleration + glTF skeletal animation under weston
# kiosk-shell on rk3576 / Mali-G52 (libmali vendor blob).
#
# Intended use on the target device: swap it in as a temporary
# violoop-edge-kiosk.service ExecStart override (systemctl edit) so it
# inherits the proven PAMName=login + TTYPath=/dev/tty7 seat/KMS
# acquisition path, then swap back to the Flutter launcher afterwards.
# Not meant to run standalone over plain SSH — weston's drm-backend needs
# a logind/seatd session for KMS, which only the systemd unit path grants.
set -uo pipefail

SPIKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${QT3D_SPIKE_BIN:-$SPIKE_DIR/build/qt3d_spike}"
ASSET="${QT3D_SPIKE_ASSET:-$SPIKE_DIR/assets/CesiumMan.glb}"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
export WAYLAND_DISPLAY=wayland-0
export QT_QPA_PLATFORM=wayland
export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
export QT_QUICK3D_FORCE_FULL_WIDTH_RENDER=1
export QSG_RHI_BACKEND=opengl
# GLES vs desktop GL and the GLES version are pinned in main.cpp via
# QSurfaceFormat (see its comment) rather than QT_OPENGL=es2 here — the
# env var alone negotiated a GLES2 context, which fails to compile
# Quick3D's default material shaders (need GLSL ES >= 3.00).
export QT_LOGGING_RULES="qt.qpa.*=true;qt.scenegraph.general=true;qt.rhi.general=true"

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

exec "$BIN" "$ASSET" >"$RUN_DIR/qt3d-spike.log" 2>&1
