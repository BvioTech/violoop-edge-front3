#!/bin/bash
# Downloads the two Khronos glTF-Sample-Models used by this spike into
# ../assets/. Both ship a baked-in skeletal (skinning) animation, which is
# the specific capability docs/display-stack-overview.md §8.4 asks a new
# rendering framework to be tested against — procedural/primitive
# animation would not exercise glTF skin + joint import at all.
#
# Not committed to git (binary test fixtures); run this once per checkout
# or per target device.
set -euo pipefail

ASSETS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/assets"
mkdir -p "$ASSETS_DIR"
cd "$ASSETS_DIR"

BASE="https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/master/2.0"

# curl -sL alone doesn't guarantee a complete file: on a flaky/throttled
# link (seen on dev1 - TLS handshakes to raw.githubusercontent.com can
# stall for minutes) it can silently write a truncated file with no
# non-zero exit code. A truncated .glb still has a valid-looking "glTF"
# magic header, so Quick3D's RuntimeLoader doesn't fail with a clear "file
# truncated" error - it fails later with the much more confusing "No
# suitable reader found for the file format", which looks like a Qt/Assimp
# build problem rather than a bad download. --fail-with-body plus a
# retry loop with -C - (resume) catches this instead of silently
# producing a bad fixture.
fetch() {
    local name="$1" url="$2" tries=5
    for ((i = 1; i <= tries; i++)); do
        if curl --fail -L --retry 3 --retry-delay 2 -C - -o "$name" "$url"; then
            break
        fi
        echo "fetch $name: attempt $i/$tries failed, retrying..." >&2
        sleep 2
    done
    # Binary glTF (.glb) header: magic "glTF" (4 bytes) + version (u32) +
    # total length (u32) at bytes 8-11, must equal the file's actual size.
    python3 - "$name" <<'PY'
import struct, sys
path = sys.argv[1]
with open(path, "rb") as f:
    data = f.read()
if len(data) < 12 or data[:4] != b"glTF":
    sys.exit(f"{path}: not a valid .glb (missing glTF magic, got {len(data)} bytes)")
declared = struct.unpack_from("<I", data, 8)[0]
if declared != len(data):
    sys.exit(f"{path}: truncated download - header says {declared} bytes, got {len(data)}")
PY
}

fetch CesiumMan.glb "$BASE/CesiumMan/glTF-Binary/CesiumMan.glb"
fetch Fox.glb "$BASE/Fox/glTF-Binary/Fox.glb"

ls -la "$ASSETS_DIR"
