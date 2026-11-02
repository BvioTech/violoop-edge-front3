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
curl -sL -o CesiumMan.glb "$BASE/CesiumMan/glTF-Binary/CesiumMan.glb"
curl -sL -o Fox.glb "$BASE/Fox/glTF-Binary/Fox.glb"

ls -la "$ASSETS_DIR"
