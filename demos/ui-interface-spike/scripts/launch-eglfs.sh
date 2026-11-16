#!/bin/bash
# EGLFS/KMS alternative to launch-under-weston.sh - no Wayland compositor,
# Qt owns the DRM output directly. Same split-GPU device-selection fix as
# ../../qt3d-spike/scripts/launch-eglfs.sh (/dev/dri/card0 is the real
# rockchip-drm KMS device; auto-detection can pick the Mali render node
# instead and fail outright). See that script's comments for the full
# story. Still relies on the systemd unit's PAMName=login +
# TTYPath=/dev/tty7 for seat/KMS access.
#
# Usage: launch-eglfs.sh [scene]
set -uo pipefail

SPIKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${UI_SPIKE_BIN:-$SPIKE_DIR/build/ui_interface_spike}"
SCENE="${1:-combined}"

QT_PREFIX="${UI_SPIKE_QT_PREFIX:-/opt/violoop-qt6}"
export LD_LIBRARY_PATH="$QT_PREFIX/lib:${LD_LIBRARY_PATH:-}"
export QML2_IMPORT_PATH="$QT_PREFIX/qml:${QML2_IMPORT_PATH:-}"
export QT_PLUGIN_PATH="$QT_PREFIX/plugins:${QT_PLUGIN_PATH:-}"

export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_INTEGRATION=eglfs_kms
# See launch-under-weston.sh for why this isn't left at the /tmp default.
export UI_SPIKE_PERF_LOG="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}/perf.csv"
export QSG_RHI_BACKEND=opengl
export QT_LOGGING_RULES="qt.qpa.*=true;qt.scenegraph.general=true;qt.rhi.general=true"

RUN_DIR="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}"
mkdir -p "$RUN_DIR"

KMS_CONFIG="$RUN_DIR/eglfs_kms.json"
cat >"$KMS_CONFIG" <<JSON
{ "device": "/dev/dri/card0" }
JSON
export QT_QPA_EGLFS_KMS_CONFIG="$KMS_CONFIG"

exec "$BIN" "$SCENE" >"$RUN_DIR/ui-interface-spike-eglfs.log" 2>&1
