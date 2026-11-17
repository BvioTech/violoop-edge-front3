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
# The version comes from ./version.txt (bumped by scripts/version.sh); the
# control file only holds VERSION_PLACEHOLDER, filled in below when the
# staged copy is written.
VERSION=$(tr -d '[:space:]' < "$APP_DIR/version.txt")
if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "error: $APP_DIR/version.txt holds '$VERSION', expected MAJOR.MINOR.PATCH" >&2
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
    "$STAGE/opt/violoop-edge-kiosk-qt/assets/away" \
    "$STAGE/etc/violoop" \
    "$STAGE/usr/bin" \
    "$STAGE/usr/lib/systemd/system" \
    "$STAGE/usr/share/doc/$PKG_NAME"

# The whole QML tree is compiled into the binary via Qt's resource system
# (qt_add_qml_module without a lazy/loose-file option), so unlike
# edge-front2's Flutter bundle there's no asset directory to copy - with
# one exception, the mascot model below.
install -m 0755 "$BIN" "$STAGE/opt/violoop-edge-kiosk-qt/violoop_edge_kiosk_qt"

# The mascot model is proprietary, so it is kept out of the GPL binary and
# shipped as a file beside it, where main.cpp looks for it (see
# CMakeLists.txt). A git-lfs checkout without the LFS content leaves a
# small text pointer here instead of the model; a binary glTF file starts
# with the four bytes "glTF".
MODEL="$APP_DIR/assets/away/VIO_DesktopPet.glb"
if [ "$(head -c 4 "$MODEL")" != "glTF" ]; then
  echo "error: $MODEL is not a binary glTF file (git-lfs content not fetched?)" >&2
  exit 1
fi
install -m 0644 "$MODEL" "$STAGE/opt/violoop-edge-kiosk-qt/assets/away/VIO_DesktopPet.glb"

sed -e "s/Architecture: ARCH_PLACEHOLDER/Architecture: $ARCH/" \
    -e "s/Version: VERSION_PLACEHOLDER/Version: $VERSION/" \
    "$APP_DIR/packaging/debian/control" > "$STAGE/DEBIAN/control"
if grep -q '_PLACEHOLDER' "$STAGE/DEBIAN/control"; then
  echo "error: unfilled placeholder left in $STAGE/DEBIAN/control" >&2
  exit 1
fi
install -m 0755 "$APP_DIR/packaging/debian/postinst" "$STAGE/DEBIAN/postinst"
install -m 0755 "$APP_DIR/packaging/debian/prerm" "$STAGE/DEBIAN/prerm"
install -m 0755 "$APP_DIR/packaging/debian/postrm" "$STAGE/DEBIAN/postrm"

install -m 0644 "$APP_DIR/packaging/etc/violoop/kiosk-qt" "$STAGE/etc/violoop/kiosk-qt"
install -m 0755 "$APP_DIR/packaging/usr/bin/violoop-edge-kiosk-qt-launch" \
    "$STAGE/usr/bin/violoop-edge-kiosk-qt-launch"
install -m 0644 "$APP_DIR/packaging/usr/lib/systemd/system/violoop-edge-kiosk-qt.service" \
    "$STAGE/usr/lib/systemd/system/violoop-edge-kiosk-qt.service"

# The license notice and where the source is (this is a GPL binary, so
# both have to travel with it). The GPL's own text is not copied in: the
# file points at /usr/share/common-licenses/GPL-3, which base-files
# provides on every Debian system.
#
# The file also names the revision this build's source is at. That is the
# tag v$VERSION only when HEAD is that tag: version.txt keeps saying
# $VERSION for every commit until the next bump, so a build of any other
# commit names the commit instead of pointing at a tag it was not built
# from.
REPO_URL=$(awk '/^Source:/{print $2; exit}' "$APP_DIR/packaging/debian/copyright")
if HEAD_SHA=$(git -C "$APP_DIR" rev-parse --verify -q HEAD 2>/dev/null); then
  TAG_SHA=$(git -C "$APP_DIR" rev-parse --verify -q "refs/tags/v$VERSION^{commit}" 2>/dev/null || true)
  if [ -n "$(git -C "$APP_DIR" status --porcelain --untracked-files=no 2>/dev/null)" ]; then
    SOURCE_REVISION="This build was made from commit $HEAD_SHA with uncommitted changes."
  elif [ "$HEAD_SHA" = "$TAG_SHA" ]; then
    SOURCE_REVISION="This build was made from the tag v$VERSION: $REPO_URL/tree/v$VERSION"
  else
    SOURCE_REVISION="This build is not a release; it was made from commit $HEAD_SHA: $REPO_URL/tree/$HEAD_SHA"
  fi
else
  SOURCE_REVISION="This build is not a release and was not made from a git checkout, so its source revision is not recorded."
fi
echo "Source revision: $SOURCE_REVISION"
sed -e "s|SOURCE_REVISION_PLACEHOLDER|$SOURCE_REVISION|" \
    "$APP_DIR/packaging/debian/copyright" > "$STAGE/usr/share/doc/$PKG_NAME/copyright"
chmod 0644 "$STAGE/usr/share/doc/$PKG_NAME/copyright"
if grep -q '_PLACEHOLDER' "$STAGE/usr/share/doc/$PKG_NAME/copyright"; then
  echo "error: unfilled placeholder left in the staged copyright file" >&2
  exit 1
fi

# mkdir gave the staged directories whatever the builder's umask allows.
# The service runs as its own user and has to reach the model under
# /opt, so do not let a strict umask on the build machine decide that.
find "$STAGE" -type d -exec chmod 0755 {} +

OUT_DIR="$APP_DIR/output"
OUT_DEB="$OUT_DIR/${PKG_NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$STAGE" "$OUT_DEB"

echo "Built $OUT_DEB"
