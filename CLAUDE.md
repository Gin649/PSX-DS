# PSX-DS: notes for Claude sessions

PSX-DS is a PlayStation emulator for dual-screen handhelds (first target: the
AYN Thor, Android; also ROCKNIX, BananaOS and the Stock Linux OS). It's a fork
of kyannon's GBA DS (https://github.com/kyannon/gba-ds), with mGBA replaced by
SwanStation (libretro, GPL-3.0). The owner is Gin649; the repo is
Gin649/PSX-DS. Keep the credit to GBA DS / kyannon in the README, the
changelog and the app's licence notes (GPL-3.0 requires it); everything else
of GBA DS's name is gone.

## How it works

- `src/emu.c` hosts SwanStation through libretro: the same functions GBA DS's
  mGBA wrapper had. A libretro core keeps its state in globals, so each open
  game (the game, the hidden copy) gets its own copy of the core library:
  Android ships two (`libswan_a.so`, `libswan_b.so`, each with its own soname,
  `scripts/set_soname.py`), passed with `emu_set_core_paths`; elsewhere
  `PSXDS_CORE` names one library, copied to a temp file per copy. Checked on
  the Thor (Gin649/ps1-dualcore-test): two copies, a state copied every
  second, 0 late frames, fast memory LUT best.
- Every PS1 frame is scaled into a fixed SCREEN_W x SCREEN_H (640x480)
  picture. PS1 RAM is read at 0x00/0x80/0xA0 + offset (2 MB). The GBA's video
  hardware functions (sprite rules, layers, HUD split, VRAM, OAM) are no-ops,
  kept until the code that calls them is reworked.
- `src/profiles.c` is the second-screen engine; its game table is empty.
  Games are added one at a time; each needs a disc ID (not read yet: every
  game's code is PSX1 for now).
- Keys: K_A right (Circle), K_B bottom (Cross), K_X top (Triangle), K_Y left
  (Square), L/R, L2/R2, L3/R3.

## Working

- Work on a branch (`psx` for now); the owner tests builds on the Thor.
- Desktop build: SDL2 in `deps/sdl-host`, SwanStation built with CMake,
  then `make`. Headless run: `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
  PSXDS_WINDOW=1024x1536 PSXDS_CORE=... build/host/psx-ds --state-dir=DIR
  --input-script=SCRIPT GAME` (script lines `TICK SHOT:file.bmp`, `TICK QUIT`).
  Without a BIOS, OpenBIOS runs (its spinning cube).
- Android: GitHub Actions (`.github/workflows/android.yml`), APK in the run's
  artifacts.
- Commit messages end with the Co-Authored-By / Claude-Session trailers the
  session gives.
