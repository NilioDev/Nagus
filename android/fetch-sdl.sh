#!/usr/bin/env bash
# Puts the SDL release that CMake builds (NGS_SDL3_TAG in cmake/NgsSDL3.cmake)
# in android/sdl, so that the Android app's Java glue code and its native SDL
# library come from the same checkout.
set -euo pipefail
here=$(dirname "$(realpath "$0")")
tag=$(sed -n 's/^set(NGS_SDL3_TAG "\([^"]*\)".*/\1/p' "$here/../cmake/NgsSDL3.cmake")
[ -n "$tag" ] || { echo "No NGS_SDL3_TAG in cmake/NgsSDL3.cmake" >&2; exit 1; }
if [ -d "$here/sdl/.git" ]; then
    have=$(git -C "$here/sdl" describe --tags --exact-match 2>/dev/null || true)
    if [ "$have" = "$tag" ]; then
        echo "android/sdl already holds $tag"
        exit 0
    fi
    rm -rf "${here:?}/sdl"
fi
git clone --depth 1 --branch "$tag" https://github.com/libsdl-org/SDL.git "$here/sdl"
echo "android/sdl now holds $tag"
