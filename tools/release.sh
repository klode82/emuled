#!/usr/bin/env bash
# Release round: reads EMULED_VERSION from CMakeLists.txt, commits the tracked changes, pushes master, tags v<version> and pushes the tag
# (the tag push starts the Release workflow).
#   tools/release.sh "commit message"      do it
#   tools/release.sh -n "commit message"   dry run: only prints what it would do
# Stops before touching anything if the tag already exists (here or on origin) or if the branch is not master.
set -euo pipefail
cd "$(dirname "$0")/.."

dry=0
if [ "${1:-}" = "-n" ]; then dry=1; shift; fi
msg="${1:-}"
[ -n "$msg" ] || { echo "usage: tools/release.sh [-n] \"commit message\"" >&2; exit 2; }

version=$(sed -n 's/^set(EMULED_VERSION \([0-9][0-9.]*\)).*/\1/p' CMakeLists.txt)
[ -n "$version" ] || { echo "EMULED_VERSION not found in CMakeLists.txt" >&2; exit 1; }
tag="v$version"

branch=$(git rev-parse --abbrev-ref HEAD)
[ "$branch" = master ] || { echo "branch is '$branch', not master" >&2; exit 1; }
if git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then echo "tag $tag already exists here" >&2; exit 1; fi
if git ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1; then echo "tag $tag already exists on origin" >&2; exit 1; fi

run() { echo "+ $*"; [ "$dry" = 1 ] || "$@"; }

echo "version $version -> tag $tag"
git status --short
# only tracked files (-a): new files must be added by hand first (git add), so nothing unwanted slips in
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then run git commit -am "$msg"; else echo "nothing to commit"; fi
run git push origin master
run git tag "$tag"
run git push origin "$tag"
if [ "$dry" = 1 ]; then echo "dry run: nothing was done"; else echo "done: Release workflow for $tag started"; fi
