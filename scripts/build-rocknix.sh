#!/bin/bash
# Reproducible cross-build of PSX-DS for ROCKNIX (aarch64) on an x86_64 Linux host.
# Produces dist/psx-ds-v<VERSION>-linux-aarch64.zip (ROCKNIX, BananaOS).
#
# Needs: git, cmake, make, zip, an x86_64 host. Everything else is fetched into ./deps.
# Set AARCH64_TOOLCHAIN_BIN to use your own aarch64-linux-gnu-gcc instead.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT="$PWD"
DEPS="$ROOT/deps"
mkdir -p "$DEPS"
. scripts/deps.sh   # SDL_BRANCH, SWAN_COMMIT, fetch_swanstation; shared with the other builds


# --- toolchain -------------------------------------------------------------
if [ -z "${AARCH64_TOOLCHAIN_BIN:-}" ]; then
	TC="$DEPS/aarch64-linux-gnu"
	if [ ! -x "$TC/bin/aarch64-linux-gnu-gcc" ]; then
		git clone --depth 1 https://github.com/radcolor/aarch64-linux-gnu "$TC"
		# The prebuilt toolchain's linker scripts hard-code its CI build path.
		for f in "$TC"/aarch64-linux-gnu/lib/*.so; do
			grep -q /home/runner "$f" 2>/dev/null &&
				sed -i "s|/home/runner/work/build-tools-gcc/build-tools-gcc/aarch64-linux-gnu/aarch64-linux-gnu/lib/|$TC/aarch64-linux-gnu/lib/|g" "$f"
		done
		# ...and its limits.h doesn't chain to glibc's (PATH_MAX etc. missing).
		L=$(echo "$TC"/lib/gcc/aarch64-linux-gnu/*/include-fixed/limits.h)
		if ! grep -qE "gbadual|psx-ds" "$L"; then  # already patched (by either name)
			{ printf '/* psx-ds: chain to glibc limits.h */\n#ifndef _GCC_LIMITS_H_\n#define _GCC_LIMITS_H_\n#include_next <limits.h>\n#endif\n'; cat "$L"; } > "$L.tmp" && mv "$L.tmp" "$L"
		fi
	fi
	export AARCH64_TOOLCHAIN_BIN="$TC/bin"
fi
CROSS="$AARCH64_TOOLCHAIN_BIN/aarch64-linux-gnu-"

# --- SwanStation (the PlayStation core, a libretro library loaded at run time) ---
fetch_swanstation "$DEPS"
cmake -S "$DEPS/swanstation" -B "$DEPS/swanstation/build-aarch64" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/aarch64.cmake" \
	-DCMAKE_SHARED_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
	-DCMAKE_BUILD_TYPE=Release
make -C "$DEPS/swanstation/build-aarch64" -j"$(nproc)" swanstation_libretro

# --- zlib (static: the devices' systems don't all have it) ---
if [ ! -d "$DEPS/zlib" ]; then
	git clone --depth 1 -b v1.3.1 https://github.com/madler/zlib.git "$DEPS/zlib"
fi
cmake -S "$DEPS/zlib" -B "$DEPS/zlib/build-aarch64" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/aarch64.cmake" -DCMAKE_BUILD_TYPE=Release
make -C "$DEPS/zlib/build-aarch64" -j"$(nproc)" zlibstatic

# --- SDL2 (only to link against; the device's own libSDL2 is used at runtime) ---
if [ ! -d "$DEPS/SDL2" ]; then
	git clone --depth 1 -b "$SDL_BRANCH" https://github.com/libsdl-org/SDL.git "$DEPS/SDL2"
fi
cmake -S "$DEPS/SDL2" -B "$DEPS/SDL2/build-aarch64" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/aarch64.cmake" \
	-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$DEPS/sdl-aarch64" -DSDL_STATIC=OFF -DSDL_TEST=OFF
make -C "$DEPS/SDL2/build-aarch64" -j"$(nproc)" install

# PSXDS_SIDE=slug: a side build of a game branch (game/<slug>), to try it
# beside the main install: the folder psx-ds-<slug> and the Ports entry
# "PSX-DS <game> testing", its own settings and states, version
# VERSION+<slug>, no updates and no EmulationStation / BananaOS integration
# (the Makefile's SIDE), and a game list of only that game's releases. The
# game's name and codes come from docs/games/<slug>.md (its title and its
# Releases row); PSXDS_SIDE_NAME / PSXDS_SIDE_CODES ("A2NE A2NP") override.
SIDE="${PSXDS_SIDE:-}"
SIDE_NAME="" SIDE_CODES=""
if [ -n "$SIDE" ]; then
	if ! [[ $SIDE =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
		echo "PSXDS_SIDE: lower-case letters, digits and dashes only (not $SIDE)" >&2
		exit 1
	fi
	RULES="docs/games/$SIDE.md"
	SIDE_NAME="${PSXDS_SIDE_NAME:-$(sed -n '1s/^# *//p' "$RULES" 2>/dev/null)}"
	SIDE_CODES="${PSXDS_SIDE_CODES:-$(grep -m1 '^| Releases |' "$RULES" 2>/dev/null | grep -o '`[A-Z0-9]\{4\}`' | tr -d '`' | tr '\n' ' ' | sed 's/ *$//')}"
	if [ -z "$SIDE_NAME" ] || [ -z "$SIDE_CODES" ]; then
		echo "PSXDS_SIDE: $RULES needs its title (# Name) and a Releases row with the codes (\`A2NE\`), or set PSXDS_SIDE_NAME and PSXDS_SIDE_CODES" >&2
		exit 1
	fi
	# (no characters SD cards' file systems refuse: "Game: Subtitle" -> "Game - Subtitle")
	SIDE_NAME="$(printf '%s' "$SIDE_NAME" | sed 's/ *: */ - /g; s#[/\\*?"<>|]##g')"
	echo "side build: $SIDE, \"PSX-DS $SIDE_NAME testing\", codes $SIDE_CODES"
fi

# --- psx-ds ---------------------------------------------------------------
make CROSS="$CROSS" ZLIB="$DEPS/zlib/build-aarch64/libz.a" ZLIB_INC="-I$DEPS/zlib -I$DEPS/zlib/build-aarch64" \
	SDL_PREFIX="$DEPS/sdl-aarch64" OUT=build/aarch64${SIDE:+-$SIDE} OPTFLAGS="-O2 -mcpu=cortex-a55" SIDE="$SIDE" SIDE_CODES="$SIDE_CODES"

# --- package ---------------------------------------------------------------
VERSION=$(cat VERSION)
APP=psx-ds${SIDE:+-$SIDE}            # the install folder
ENTRY="PSX-DS${SIDE:+ $SIDE_NAME testing}"   # the Ports entry
ZIP="psx-ds-v$VERSION${SIDE:+-$SIDE}-linux-aarch64.zip"
PKG="$ROOT/build/pkg/$APP"
rm -rf "$ROOT/build/pkg"
mkdir -p "$PKG"
"${CROSS}strip" -o "$PKG/psx-ds" build/aarch64${SIDE:+-$SIDE}/psx-ds
cp dist/launch.sh dist/es-play.sh dist/screens.sh dist/psx-ds.cfg dist/gamecontrollerdb.txt dist/banana-play.sh "$PKG/"
mkdir -p "$PKG/logos"
printf "Optional: your own logo images (<CODE>.png or .bmp) override the ones extracted from the games.\n" > "$PKG/logos/README.txt"
mkdir -p "$PKG/cheats"
printf "RetroArch cheat files (.cht) for PSX-DS, in this folder or subfolders: picked by name, as RetroArch does (Game (USA).gba -> Game (USA) (Code Breaker).cht).\nRetroArch's own cheat folders are searched too (cheat_dir in psx-ds.cfg).\nSwitch cheats on in the pause menu: Extras > Cheats. More in the wiki: https://github.com/Gin649/PSX-DS/wiki/Playing#cheats\n" > "$PKG/cheats/README.txt"
mkdir -p "$PKG/overlays"
cp dist/overlays-README.txt "$PKG/overlays/README.txt"
mkdir -p "$PKG/patches"
printf "Patches for PSX-DS (.ips, .ups, .bps: romhacks, colour patches, translations), in a folder for each game,\nnamed after the game and its region (for example \"Mario & Luigi - Superstar Saga (USA)\"). PSX-DS makes a game's folder\nthe first time you play it; your ROM itself is never changed.\nSwitch patches on in the pause menu: Extras > Patches. More in the wiki: https://github.com/Gin649/PSX-DS/wiki/Playing#patches\n" > "$PKG/patches/README.txt"
cp README.md "$PKG/README.md"
# mGBA is statically linked (MPL-2.0): ship its licence and where to get its source
mkdir -p "$PKG/licenses"
cp LICENSE "$PKG/licenses/PSX-DS-LICENSE.txt"
cp "$DEPS/swanstation/LICENSE" "$PKG/licenses/SwanStation-LICENSE.txt"
cp assets/fonts/Jost-OFL.txt "$PKG/licenses/Jost-OFL.txt"
cp third_party/rcheevos/LICENSE "$PKG/licenses/rcheevos-LICENSE.txt"
cp third_party/libwebp/COPYING "$PKG/licenses/libwebp-COPYING.txt"
cp third_party/libwebp/PATENTS "$PKG/licenses/libwebp-PATENTS.txt"
printf "PSX-DS %s is free software under the GNU General Public License, version 3 only (PSX-DS-LICENSE.txt).\nIt is a modified version of GBA DS (c) kyannon: https://github.com/kyannon/gba-ds\nPlayStation emulation: SwanStation (c) its authors, GPL-3.0 (SwanStation-LICENSE.txt): https://github.com/libretro/swanstation (commit %s)\nzlib (c) Jean-loup Gailly and Mark Adler, zlib licence.\nIts text is set in Jost (c) The Jost Project Authors, SIL Open Font License 1.1 (Jost-OFL.txt).\nRetroAchievements support: rcheevos (c) RetroAchievements, MIT (rcheevos-LICENSE.txt), %s.\nWebP images: libwebp (c) Google Inc., BSD 3-clause (libwebp-COPYING.txt, libwebp-PATENTS.txt), v1.6.0.\nPSX-DS source: https://github.com/Gin649/PSX-DS\n" "$VERSION" "$SWAN_COMMIT" "$(tail -1 third_party/rcheevos/VERSION)" > "$PKG/licenses/README.txt"
# two copies of the core: the game and the hidden copy each load their own (emu.c)
mkdir -p "$PKG/cores"
for c in a b; do
	"${CROSS}strip" --strip-unneeded -o "$PKG/cores/libswan_$c.so" "$DEPS/swanstation/build-aarch64/swanstation_libretro.so"
done
sed "s#/psx-ds/#/$APP/#g; s#psx-ds-error.log#$APP-error.log#" "dist/PSX-DS.sh" > "$ROOT/build/pkg/$ENTRY.sh"
# EmulationStation artwork for the Ports entry
mkdir -p "$ROOT/build/pkg/images"
cp assets/brand/psx-ds-icon.png "$ROOT/build/pkg/images/$ENTRY.png"
# and inside the app's folder, for menus that install the folder alone (BananaOS's
# Store: its catalog entry's "menu" > "media" > "image" names images/icon.png)
mkdir -p "$PKG/images"
cp assets/brand/psx-ds-icon-transparent.png "$PKG/images/icon.png"
chmod +x "$PKG"/*.sh "$PKG/psx-ds" "$ROOT/build/pkg/$ENTRY.sh"
if [ -n "$SIDE" ]; then
	rm -f "$ROOT"/dist/psx-ds-*-"$SIDE"-linux-aarch64.zip
else
	rm -f "$ROOT"/dist/psx-ds-*rocknix-aarch64.zip "$ROOT"/dist/psx-ds-v*[0-9]-linux-aarch64.zip "$ROOT"/dist/psx-ds-v*-dev-linux-aarch64.zip
fi
(cd "$ROOT/build/pkg" && zip -qr "$ROOT/dist/$ZIP" "$APP" "$ENTRY.sh" images)
echo "built dist/$ZIP"
