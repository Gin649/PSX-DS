# Versions of what the builds fetch into deps/ (sourced by the build scripts).
SDL_BRANCH=release-2.30.x
# SwanStation (libretro/swanstation, GPL-3.0), the PlayStation core
SWAN_COMMIT=631f0fb31b809e0f889d7c551fe695788e39c1d3

# fetch_swanstation DEPS: clones SwanStation at SWAN_COMMIT into DEPS/swanstation
fetch_swanstation() {
	if [ ! -d "$1/swanstation" ]; then
		git clone https://github.com/libretro/swanstation.git "$1/swanstation"
		git -C "$1/swanstation" checkout "$SWAN_COMMIT"
	fi
}
