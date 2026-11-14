#!/bin/bash
# Launches a locally built app straight on KMS (eglfs), with no weston -
# the counterpart of launch-under-weston.sh, for comparing the two on a
# real device. The packaged launcher does the same when EDGE_KIOSK_QPA=eglfs
# (../packaging/usr/bin/violoop-edge-kiosk-qt-launch); see there for why
# the KMS device is named and the cursor hidden.
#
# Intended use: swap in as a temporary ExecStart override of the kiosk
# service (systemctl edit) - it needs that unit's PAMName=login +
# TTYPath=/dev/tty7, not plain SSH.
set -uo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${EDGE_QT_BIN:-$APP_DIR/build/violoop_edge_kiosk_qt}"

QT_PREFIX="${EDGE_QT_PREFIX:-/opt/violoop-qt6}"
export LD_LIBRARY_PATH="$QT_PREFIX/lib:${LD_LIBRARY_PATH:-}"
export QML2_IMPORT_PATH="$QT_PREFIX/qml:${QML2_IMPORT_PATH:-}"
export QT_PLUGIN_PATH="$QT_PREFIX/plugins:${QT_PLUGIN_PATH:-}"

export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_INTEGRATION=eglfs_kms
export QT_QPA_EGLFS_HIDECURSOR=1
export QSG_RHI_BACKEND=opengl
export QT_LOGGING_RULES="qt.qpa.*=true;qt.scenegraph.general=true"

RUN_DIR="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}"
mkdir -p "$RUN_DIR"

KMS_CONFIG="$RUN_DIR/eglfs_kms.json"
printf '{ "device": "/dev/dri/card0" }\n' >"$KMS_CONFIG"
export QT_QPA_EGLFS_KMS_CONFIG="$KMS_CONFIG"

exec "$BIN" >"$RUN_DIR/app.log" 2>&1
