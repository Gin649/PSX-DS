#!/bin/bash
# Builds the Android APK (arm64-v8a): dist/psx-ds-vX.Y.Z-android.apk.
#
# Needs: the Android SDK (ANDROID_HOME or ANDROID_SDK_ROOT) with an NDK
# (ANDROID_NDK_HOME, or ANDROID_NDK_LATEST_HOME as on GitHub's runners),
# Gradle 8.7+ and JDK 17, git, cmake, ninja. SwanStation and SDL2 are fetched
# into ./deps. The Claude workspace can't download the SDK, so this runs on
# GitHub Actions (.github/workflows/android.yml).
#
# Release signing: set PSXDS_KEYSTORE (path), PSXDS_KEYSTORE_PASSWORD,
# PSXDS_KEY_ALIAS, PSXDS_KEY_PASSWORD; without them the APK is debug-signed.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
DEPS="$ROOT/deps"
mkdir -p "$DEPS"
. scripts/deps.sh   # SDL_BRANCH, SWAN_COMMIT, fetch_swanstation
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_LATEST_HOME:-}}"
if [ -z "$NDK" ] || [ ! -f "$NDK/build/cmake/android.toolchain.cmake" ]; then
	echo "set ANDROID_NDK_HOME to an Android NDK" >&2
	exit 1
fi
export ANDROID_NDK_HOME="$NDK"
ABI=arm64-v8a
API=26
# any arm64 (ARMv8.0): -mcpu for a newer core would let in instructions older chips lack
CPU_FLAGS="-march=armv8-a"

# --- sources -----------------------------------------------------------------
fetch_swanstation "$DEPS"
if [ ! -d "$DEPS/SDL2" ]; then
	git clone --depth 1 -b "$SDL_BRANCH" https://github.com/libsdl-org/SDL.git "$DEPS/SDL2"
fi

# --- SwanStation: two copies of its libretro core ------------------------------
# A libretro core keeps its state in globals, so the game and the hidden copy
# each load a library of their own (emu.c), each with its own soname.
SWAN_BUILD="$DEPS/swanstation/build-android-$ABI"
cmake -S "$DEPS/swanstation" -B "$SWAN_BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
	-DANDROID_ABI=$ABI -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release
cmake --build "$SWAN_BUILD" -j"$(nproc)" --target swanstation_libretro
JNI_OUT=android/app/src/main/jniLibs/$ABI
mkdir -p "$JNI_OUT"
for c in a b; do
	cp "$SWAN_BUILD/swanstation_libretro_android.so" "$JNI_OUT/libswan_$c.so"
	"$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-unneeded "$JNI_OUT/libswan_$c.so"
	python3 scripts/set_soname.py "$JNI_OUT/libswan_$c.so" "libswan_$c.so"
done

# --- APK ---------------------------------------------------------------------------
# licences, inside the APK (assets/licenses)
VERSION=$(cat VERSION)
LIC=android/app/src/main/assets/licenses
rm -rf "$LIC" && mkdir -p "$LIC"
cp LICENSE "$LIC/PSX-DS-LICENSE.txt"
cp "$DEPS/swanstation/LICENSE" "$LIC/SwanStation-LICENSE.txt"
cp "$DEPS/SDL2/LICENSE.txt" "$LIC/SDL2-LICENSE.txt"
cp assets/fonts/Jost-OFL.txt "$LIC/Jost-OFL.txt"
cp third_party/rcheevos/LICENSE "$LIC/rcheevos-LICENSE.txt"
cp third_party/libwebp/COPYING "$LIC/libwebp-COPYING.txt"
cp third_party/libwebp/PATENTS "$LIC/libwebp-PATENTS.txt"
printf "PSX-DS %s is free software under the GNU General Public License, version 3 only (PSX-DS-LICENSE.txt).\nIt is a modified version of GBA DS (c) kyannon: https://github.com/kyannon/gba-ds\nPlayStation emulation: SwanStation (c) its authors, GPL-3.0 (SwanStation-LICENSE.txt): https://github.com/libretro/swanstation (commit %s)\nSDL2 (c) Sam Lantinga, zlib licence: https://github.com/libsdl-org/SDL (%s)\nIts text is set in Jost (c) The Jost Project Authors, SIL Open Font License 1.1 (Jost-OFL.txt).\nRetroAchievements support: rcheevos (c) RetroAchievements, MIT (rcheevos-LICENSE.txt), %s.\nWebP images: libwebp (c) Google Inc., BSD 3-clause (libwebp-COPYING.txt, libwebp-PATENTS.txt), v1.6.0.\nPSX-DS source: https://github.com/Gin649/PSX-DS\n" \
	"$VERSION" "$(git -C "$DEPS/swanstation" rev-parse --short HEAD)" "$SDL_BRANCH" "$(tail -1 third_party/rcheevos/VERSION)" > "$LIC/README.txt"

gradle -p android --no-daemon assembleRelease -PsdlDir="$DEPS/SDL2" -PcpuFlags="$CPU_FLAGS"

APK="dist/psx-ds-v$VERSION-android.apk"
mkdir -p dist
rm -f dist/psx-ds-*-android.apk
cp android/app/build/outputs/apk/release/app-release.apk "$APK"
echo "built $APK"
