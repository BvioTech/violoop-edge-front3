#!/bin/bash
# Installs the Qt6 packages this spike needs, on the target device
# (Debian 12 bookworm, aarch64 — see docs/display-stack-overview.md §1).
# Run with network access to deb.debian.org (bookworm/main has Qt 6.4.2
# including Quick3D and Wayland — no backports/third-party repo needed).
#
# Gotcha hit on dev1 (2026-11): deb.debian.org was unreachable directly
# from both dev1 and the dev workstation on that network (`apt-get update`
# worked, but package downloads failed with "Network is unreachable" /
# "Unable to connect", both over IPv4 and IPv6). Worked around by
# downloading the .debs on a machine that *did* have a route (via an HTTP
# proxy) and `scp`-ing them to dev1, then running
# `apt-get install -y ./*.deb` from that local directory (apt resolves
# deps from local files fine). If this recurs, that is the fallback —
# don't burn time retrying the direct download.
#
# Package list below is also the result of trial and error: qt6-quick3d-dev
# pulls in the Quick3D-specific QML modules, but NOT the base
# QtQuick/QtQml/QtQuick.Window modules (those are separate `qml6-module-*`
# packages in Debian's split packaging) — omitting them fails at QML load
# time with "module \"QtQuick\" is not installed" / "...WorkerScript...".
set -euo pipefail

PACKAGES=(
    # Build toolchain (skip if already present).
    cmake g++ make ninja-build pkg-config

    # Qt6 base + dev headers.
    qt6-base-dev qt6-base-dev-tools

    # Wayland QPA platform plugin (equivalent of Flutter's GDK_BACKEND=wayland path).
    qt6-wayland qt6-wayland-dev

    # QML/Quick engine + dev tools (headers, qmlplugindump, etc. — note:
    # the generic `qml` script runner binary is NOT packaged here).
    qt6-declarative-dev qt6-declarative-dev-tools

    # Quick3D (glTF/skeletal-animation rendering) + its runtime QML modules.
    qt6-quick3d-dev qt6-quick3d-dev-tools qt6-shadertools-dev
    qml6-module-quick3d qml6-module-quick3d-assetutils qml6-module-quick3d-helpers

    # Base QtQuick/QtQml QML modules Quick3D's RuntimeLoader transitively
    # needs at runtime (not pulled in by qt6-quick3d-dev automatically).
    qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript
    qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-layouts
    qml6-module-qtquick-timeline qml6-module-qtquick-shapes
    qml6-module-qt-labs-qmlmodels qml6-module-qt-labs-sharedimage
    qml6-module-qt-labs-wavefrontmesh
)

sudo apt-get update
sudo apt-get install -y "${PACKAGES[@]}"
