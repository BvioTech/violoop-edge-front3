#!/bin/bash
# Bumps ./version, commits it, tags the commit v<version> and pushes both.
# The tag push is what triggers .github/workflows/build-release.yml.
#
# Usage: scripts/version.sh [major|minor|patch]   (default: patch)
#        DRY_RUN=1 scripts/version.sh [...]       (print the next version only)
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
VERSION_PATH=version

if [ $# -gt 1 ]; then
  echo "error: expected at most one argument (major|minor|patch)" >&2
  exit 1
fi

CURRENT=$(tr -d '[:space:]' < "$VERSION_PATH")
if ! [[ "$CURRENT" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
  echo "error: $VERSION_PATH holds '$CURRENT', expected MAJOR.MINOR.PATCH" >&2
  exit 1
fi
MAJOR=${BASH_REMATCH[1]}
MINOR=${BASH_REMATCH[2]}
PATCH=${BASH_REMATCH[3]}

case "${1:-patch}" in
  major) MAJOR=$((MAJOR + 1)); MINOR=0; PATCH=0 ;;
  minor) MINOR=$((MINOR + 1)); PATCH=0 ;;
  patch) PATCH=$((PATCH + 1)) ;;
  *) echo "error: unknown argument '$1' (major|minor|patch)" >&2; exit 1 ;;
esac

VERSION="$MAJOR.$MINOR.$PATCH"
TAG="v$VERSION"
echo "Version: $CURRENT -> $VERSION"

if [ -n "${DRY_RUN:-}" ]; then
  exit 0
fi

# Only the version file goes into the release commit, so anything else
# that is uncommitted would be left out of the tagged tree unnoticed.
if [ -n "$(git status --porcelain)" ]; then
  echo "error: working tree is not clean - commit or stash first" >&2
  exit 1
fi
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  echo "error: tag $TAG already exists" >&2
  exit 1
fi
BRANCH=$(git symbolic-ref --short HEAD)

echo "$VERSION" > "$VERSION_PATH"
git add "$VERSION_PATH"
git commit -m "$TAG"
git tag "$TAG"
git push --atomic origin "$BRANCH" "$TAG"
