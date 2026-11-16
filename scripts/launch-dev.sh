#!/bin/bash
# Runs a locally built app on a real device, through the same launcher
# the .deb ships (../packaging/usr/bin/violoop-edge-kiosk-qt-launch), so
# that what is tried out here is what the service will do.
#
# Usage: launch-dev.sh [wayland|eglfs]
#   wayland (the default) runs the app under weston; eglfs runs it
#   straight on KMS with no compositor, for comparing the two.
#
# Intended use: swap in as a temporary ExecStart override of the kiosk
# service (systemctl edit) - it needs that unit's PAMName=login +
# TTYPath=/dev/tty7, not plain SSH.
#
# What differs from the shipped service:
#   - the binary is ./build's (EDGE_QT_BIN to name another), and
#     EDGE_QT_PREFIX can point at another Qt;
#   - Qt logs its platform plugin and scene graph setup, and weston runs
#     with --debug;
#   - the app's output goes to app.log, next to weston.log, in the unit's
#     runtime directory (/tmp/violoop-edge-kiosk when the unit has none).
set -uo pipefail

# See the launcher: SIGUSR1 and SIGUSR2 are commands to the app and must
# not kill this script before the exec.
trap '' USR1 USR2

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

QPA="${1:-wayland}"
case "$QPA" in
    wayland|eglfs) ;;
    *) echo "usage: $0 [wayland|eglfs]" >&2; exit 2 ;;
esac

export EDGE_KIOSK_QPA="$QPA"
export EDGE_QT_BIN="${EDGE_QT_BIN:-$APP_DIR/build/violoop_edge_kiosk_qt}"
export EDGE_KIOSK_WESTON_ARGS="--debug"
export QT_LOGGING_RULES="${QT_LOGGING_RULES:-qt.qpa.*=true;qt.scenegraph.general=true}"
export RUNTIME_DIRECTORY="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}"
mkdir -p "$RUNTIME_DIRECTORY"

exec "$APP_DIR/packaging/usr/bin/violoop-edge-kiosk-qt-launch" >"$RUNTIME_DIRECTORY/app.log" 2>&1
