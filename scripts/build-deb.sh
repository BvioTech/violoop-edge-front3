#!/bin/bash
# Builds the violoop-edge-kiosk-qt .deb - mirrors
# ../../edge-front2/scripts/build-deb.sh's structure (same staging-tree +
# plain `dpkg-deb --build` approach, no debian/rules or dpkg-buildpackage
# here either), adapted for a CMake/Qt app instead of a Flutter bundle.
#
# Usage: build-deb.sh [arch]   (arch defaults to the host's dpkg arch;
#   only arm64 is meaningful - violoop-qt6, the Qt runtime this links
#   against, only ships an arm64 build)
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCH="${1:-$(dpkg --print-architecture)}"
case "$ARCH" in
  arm64) ;;
  *) echo "error: unsupported architecture '$ARCH' (only arm64 is built - violoop-qt6 is arm64-only)" >&2; exit 2 ;;
esac

QT_PREFIX="${VIOLOOP_QT_PREFIX:-/opt/violoop-qt6}"
if [ ! -d "$QT_PREFIX" ]; then
  echo "error: $QT_PREFIX not found - install the violoop-qt6 package first (apt-get install violoop-qt6)" >&2
  exit 2
fi

PKG_NAME=$(awk -F': *' '/^Package:/{print $2; exit}' "$APP_DIR/packaging/debian/control")
# The version comes from ./version (bumped by scripts/version.sh), not
# from the control file, whose Version: line is only a placeholder.
VERSION=$(tr -d '[:space:]' < "$APP_DIR/version")
if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "error: $APP_DIR/version holds '$VERSION', expected MAJOR.MINOR.PATCH" >&2
  exit 2
fi

echo "Building $PKG_NAME $VERSION for $ARCH"

# Native build only (same assumption edge-front2's script makes for
# arm64: CI runs on a native arm64 GitHub-hosted runner, no cross
# toolchain). Build dir name includes the arch for a mild sanity check if
# this is ever run on the wrong host.
BUILD_DIR="$APP_DIR/build-$ARCH"
cmake -S "$APP_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD_DIR"

BIN="$BUILD_DIR/violoop_edge_kiosk_qt"
if [ ! -x "$BIN" ]; then
  echo "error: build did not produce $BIN" >&2
  exit 1
fi

STAGE="$APP_DIR/output/stage-$ARCH"
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" \
    "$STAGE/opt/violoop-edge-kiosk-qt" \
    "$STAGE/etc/violoop" \
    "$STAGE/usr/bin" \
    "$STAGE/usr/lib/systemd/system"

# The whole QML tree is compiled into the binary via Qt's resource system
# (qt_add_qml_module without a lazy/loose-file option), so unlike
# edge-front2's Flutter bundle there's no separate asset directory to
# copy - just the one executable.
install -m 0755 "$BIN" "$STAGE/opt/violoop-edge-kiosk-qt/violoop_edge_kiosk_qt"

sed -e "s/Architecture: ARCH_PLACEHOLDER/Architecture: $ARCH/" \
    -e "s/^Version: .*/Version: $VERSION/" \
    "$APP_DIR/packaging/debian/control" > "$STAGE/DEBIAN/control"
install -m 0755 "$APP_DIR/packaging/debian/postinst" "$STAGE/DEBIAN/postinst"
install -m 0755 "$APP_DIR/packaging/debian/prerm" "$STAGE/DEBIAN/prerm"
install -m 0755 "$APP_DIR/packaging/debian/postrm" "$STAGE/DEBIAN/postrm"

install -m 0644 "$APP_DIR/packaging/etc/violoop/kiosk-qt" "$STAGE/etc/violoop/kiosk-qt"
install -m 0755 "$APP_DIR/packaging/usr/bin/violoop-edge-kiosk-qt-launch" \
    "$STAGE/usr/bin/violoop-edge-kiosk-qt-launch"
install -m 0644 "$APP_DIR/packaging/usr/lib/systemd/system/violoop-edge-kiosk-qt.service" \
    "$STAGE/usr/lib/systemd/system/violoop-edge-kiosk-qt.service"

OUT_DIR="$APP_DIR/output"
OUT_DEB="$OUT_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$STAGE" "$OUT_DEB"

echo "Built $OUT_DEB"
