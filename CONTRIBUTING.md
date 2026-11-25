# Contributing

## Clean-room rules

Nagus is an independent, from-scratch implementation.

1. **Write your own code.** Do not copy or adapt code from other
   reimplementations, remakes, mods or tools for the original game, whatever
   their license. They may be read only as documentation of a file format;
   describe what you learned in your own words in `docs/formats/` and
   implement from that description.
2. **No decompiled code.** A disassembler or debugger may be used to observe
   how the original program behaves. Record the behaviour as a specification,
   never paste decompiler output into the source tree.
3. **No game data.** Never commit original data files, executables, or
   anything extracted from them (pictures, fonts, layouts, sounds, films,
   text, rule tables). Tests that need game data must skip when the files are
   not present.
4. **No trademarks.** Do not use the original game's title, its publisher's
   or the licensor's names or logos in the program, the packages or the
   store pages. Saying which game Nagus plays, in plain text in the README,
   is fine.

## Layout

| Path | What |
| --- | --- |
| `src/core` | Engine core. Platform independent, no SDL, fully unit-testable: reading and importing the game files, the screens, drawing into an 800×600 picture. |
| `src/app` | Platform layer and game executable (SDL3): window, input, import screen. |
| `tests` | Unit tests for the core, and `ngs_snapshot` that draws the menus to `.bmp` files. |
| `android` | The Android app (Gradle) around the same code. |
| `packaging` | Linux desktop files, icon and AppImage script. |
| `docs` | Our own documentation: the file formats in `docs/formats`, the original's behaviour in `docs/rules`, and how the engine is built in `docs/architecture.md`. |

## Code style

C++20. Warnings are errors in CI (`-DNGS_WARNINGS_AS_ERRORS=ON`). Every source
file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Building

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

SDL3 is used from the system when found, otherwise the pinned release
(`NGS_SDL3_TAG` in `cmake/NgsSDL3.cmake`) is downloaded and built
(`-DNGS_USE_SYSTEM_SDL3=OFF` forces that). `-DNGS_BUILD_APP=OFF` builds only
the core and the tests, without SDL.

Put your game (CD image or zip) and `botf102e.zip` in `gamedata/` to run the
tests that need them; the first test run imports them into
`build/test-library`.

## Linux AppImage

`packaging/linux/make-appimage.sh build out` turns a Release build with SDL
linked in (`-DNGS_USE_SYSTEM_SDL3=OFF`) into
`out/Nagus-<version>-linux-<arch>.AppImage`. CI makes one on every run
(artifact `Nagus-Linux-AppImage`). The icon in `packaging/linux` is our own
art.

## Android

Needs the Android SDK with NDK (Android Studio installs both), JDK 17 and
Gradle 8.12 or newer.

```sh
android/fetch-sdl.sh                      # SDL's Java code, same release as CMake builds
gradle -p android assembleRelease         # or open android/ in Android Studio
```

The APK lands in `android/app/build/outputs/apk/release/`. It is signed with
the debug key unless `NAGUS_KEYSTORE`, `NAGUS_KEYSTORE_PASSWORD`,
`NAGUS_KEY_ALIAS` and `NAGUS_KEY_PASSWORD` name a release key. CI builds the
APK on every run (artifact `Nagus-Android-APK`).

## Making a release

`.github/workflows/release.yml` builds Linux (AppImage), Windows, macOS and
Android packages and puts them on a GitHub release. Set `VERSION` in
`CMakeLists.txt`, merge that to `main`, then either push a tag `v<version>`
or run the *Release* workflow by hand from the Actions tab with that tag. The
workflow stops if the tag and `VERSION` differ. Releases are marked as
pre-releases for now.
