#!/bin/bash
# Alternative to launch-under-weston.sh: run the Qt binary directly on
# EGLFS/KMS, with no Wayland compositor at all — Qt owns the DRM output
# itself. Tests the other half of display-stack-overview.md §8.1 ("Qt 的
# Wayland 或 EGLFS QPA 插件"). Written because the wayland-egl QPA path
# negotiated a context whose QSurfaceFormat.version() is stuck at 2.0
# (confirmed via qt.rhi.general logging) even though the underlying
# libmali context is GLES 3.2 (confirmed via GL_VERSION) — Quick3D's
# shader codegen trusts the former, so skinned-mesh shaders using
# textureSize() fail to compile ("textureSize is not supported in ESSL
# 100") despite the hardware being capable. Still relies on the systemd
# unit's PAMName=login + TTYPath=/dev/tty7 for seat/KMS access.
set -uo pipefail

SPIKE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${QT3D_SPIKE_BIN:-$SPIKE_DIR/build/qt3d_spike}"
ASSET="${QT3D_SPIKE_ASSET:-$SPIKE_DIR/assets/CesiumMan.glb}"

export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_INTEGRATION=eglfs_kms
export QSG_RHI_BACKEND=opengl
export QT_LOGGING_RULES="qt.qpa.*=true;qt.scenegraph.general=true;qt.rhi.general=true"

RUN_DIR="${RUNTIME_DIRECTORY:-/tmp/violoop-edge-kiosk}"
mkdir -p "$RUN_DIR"

# This device has a split GPU/display setup (see
# docs/display-stack-overview.md §2): /dev/dri/card0 is rockchip-drm (the
# real KMS/scanout device for DSI-1), /dev/dri/card1 is the Mali render
# node. eglfs_kms's auto-detection picks whichever card node comes back
# first from udev, which here was card1 — that fails modesetting outright
# ("drmModeGetResources failed (Operation not supported)") and Qt aborts
# with "Cannot create window: no screens available". Force the right
# device explicitly via a KMS config file.
KMS_CONFIG="$RUN_DIR/eglfs_kms.json"
cat >"$KMS_CONFIG" <<JSON
{ "device": "/dev/dri/card0" }
JSON
export QT_QPA_EGLFS_KMS_CONFIG="$KMS_CONFIG"

exec "$BIN" "$ASSET" >"$RUN_DIR/qt3d-spike-eglfs.log" 2>&1
