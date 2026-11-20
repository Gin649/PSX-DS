# PSX-DS

A PlayStation emulator for dual-screen handhelds, made first for the **AYN Thor**. The game runs on one screen, and the other shows the game's own map or menu, kept up to date as you play, the way the DS Castlevanias work. The second screen is filled from a hidden second copy of the game, so your game is never paused, sent input or changed.

**Status: early work in progress.** PS1 games run (SwanStation, with save states, memory cards, cheats and sound); second-screen support is added one game at a time, starting with Symphony of the Night.

PSX-DS is a fork of **[GBA DS](https://github.com/kyannon/gba-ds) by kyannon**, which does the same for Game Boy Advance games: its app, menus, second-screen engine and ports are kyannon's work. PSX-DS swaps its emulator for **[SwanStation](https://github.com/libretro/swanstation)** (a libretro PlayStation core, GPL-3.0) and is set up for PS1 games only.

## Platforms

- **Android** (AYN Thor and other dual-screen Android handhelds)
- **ROCKNIX**, **BananaOS** and the **Stock Linux OS** on dual-screen Linux handhelds

You need a PS1 BIOS (such as `scph5501.bin`) for most games; without one, SwanStation's built-in OpenBIOS is used, and some games won't start.

## Building

- **Desktop** (for development): build SDL2 into `deps/` (`scripts/setup-host.sh`), build SwanStation's libretro core, then `make`. Run with `PSXDS_CORE=path/to/swanstation_libretro.so build/host/psx-ds GAME.cue`.
- **Android**: `scripts/build-android.sh` (needs the Android SDK and NDK; GitHub Actions runs it, `.github/workflows/android.yml`). It builds SwanStation and packs two copies of it, one for the game and one for the hidden copy.
- **ROCKNIX / BananaOS**: `scripts/build-rocknix.sh`.

## Licence

GPL-3.0 only (see LICENSE), as GBA DS is. PSX-DS is a modified version of GBA DS (c) kyannon. SwanStation (c) its authors, GPL-3.0. SDL2 (zlib licence), Jost (SIL OFL), rcheevos (MIT) and libwebp (BSD) keep their own licences.
