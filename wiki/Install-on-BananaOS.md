[BananaOS](https://github.com/JorreFog/BananaLinux) is a light Linux made only for the RG DS and RG DS Plus. PSX-DS runs on it as a port, from the same zip as on ROCKNIX. *Support is new: report anything that doesn't work (see [Troubleshooting](Troubleshooting)).*

## Install

1. Download the latest `psx-ds-v…-linux-aarch64.zip` from the [Releases page](https://github.com/Gin649/PSX-DS/releases).
2. Unzip it into the card's `roms/ports` folder (on the device: `/data/roms/ports`). You get a `psx-ds/` folder, a `PSX-DS.sh` entry and `images/`. Banana's file sharing (Settings → Network) lets you copy it over Wi-Fi.
3. Keep your PlayStation games in `roms/psx` (`/data/roms/psx`), where Banana's own PS1 list finds them too. On the RG DS Plus, games in the second SD card's `roms/psx` folder are listed as well, as long as Banana's Settings shows that card's games (Second SD card: Show its games, the default).
4. In Banana's menu, open **Ports** and start **PSX-DS**.

## How it fits in

- **Both screens:** Banana has no compositor, so PSX-DS opens a window on each panel and draws them with the GPU, straight onto the panels (SDL's KMS/DRM driver); touches on the bottom panel work as on ROCKNIX. Banana's ports launcher normally switches the bottom panel off; PSX-DS switches it back on while it runs.
- **Buttons:** the zip has a mapping for the RG DS's built-in controls (`psx-ds/gamecontrollerdb.txt`, ROCKNIX's), since Banana ships none for SDL apps. The buttons work as on ROCKNIX (HOME, or a tap on the bottom touchscreen, opens the pause menu).
- **Where things go:** these replace ROCKNIX's paths unless you've changed them in `psx-ds.cfg`:

| | |
|---|---|
| Games | `/data/roms/psx`, and the second SD card's `roms/psx` (`/run/sd2/roms/psx`) while it's in. A card put in while PSX-DS is open is read the next time it starts. |
| Battery saves (`.srm`) | `/data/saves/gba`: the same file BananaOS's own PlayStation list uses, so a game goes on from where you left it in either. PSX-DS's saves from before v1.4.10 (`/data/saves/<game>.sav`) are carried over the first time you start each game; if both have a save, the newer one is used and the other is kept as `.srm.bak`. |
| Screenshots | `/data/screenshots` |
| Cheats | `roms/ports/psx-ds/cheats` (RetroArch `.cht` files; subfolders are fine), and RetroArch's `/data/retroarch/cheats` |
| Patches | `roms/ports/psx-ds/patches/<game>` (a folder per game, made by PSX-DS) |
| Save states, settings from the menu, logos | `roms/ports/psx-ds/states/` |
| Log | `roms/ports/psx-ds/psx-ds.log` |

To keep PSX-DS's own files (states, cheats, overlays, logos, patches, bug reports) somewhere else, see [Keeping PSX-DS's files somewhere else](Install-on-ROCKNIX#keeping-psx-dss-files-somewhere-else): it works the same here.

- **RetroAchievements:** the account from Banana's first-time setup (`banana/accounts.conf`) is used, in casual mode. Banana has no `curl` program, so PSX-DS talks to the server through Banana's curl library instead (the log says "using libcurl").
- **Battery and clock** in the pause menu come from the system, as on ROCKNIX.
- **Back to Banana:** closing PSX-DS from Ports goes back to Banana's menu.

## Upgrading

In PSX-DS's game list, **Settings** > **Updates** > **Check now**, then **Update now** (see [Playing](Playing#updates)). Or unzip the new release over the old one in `roms/ports`.

## Starting games from Banana's PlayStation list

PSX-DS can take over the second-screen games in Banana's own **PlayStation** list, so you don't have to open PSX-DS first:

1. Start PSX-DS from Ports, open **Settings** in its game list (tap it, top right, or press SELECT) and switch **PSX-DS in BananaOS's PS1 list** on.
2. Restart BananaOS (Settings → System), since Banana reads its systems list when it starts.

Then a game picked in Banana's PlayStation list starts in PSX-DS when PSX-DS has second-screen support for it (see [[Supported games]]), and closing it goes back to Banana's menu. Every other game plays in RetroArch (gpSP) as before, but on its own: without Banana's own second screen beside it (its game panel, touch tiles and the Pokémon and game companions). Banana decides how to share the two screens from the list's start line before the game starts, and with this on that line is PSX-DS's, which needs both screens. If you use Banana's second screen with PS1 games PSX-DS doesn't support, leave this off and start PSX-DS from Apps or Ports instead. (A way for Banana to pick the line game by game, which would end this, has been asked of its developers.)

How it works: Banana reads its systems from `banana/systems.conf` on the card when there is one, instead of its built-in list. Switching this on makes that copy (or edits yours) so the PlayStation entry runs `psx-ds/banana-play.sh`, keeping the original line as a comment (`# psx-ds: run = ...`), which `banana-play.sh` uses for the games PSX-DS doesn't support. Switching it off puts the line back, and deletes the copy if PSX-DS made it and nothing else in it changed.

While the copy is there, systems added by later BananaOS updates don't appear in Banana's menu until you switch this off (Banana's own list is always in `banana/systems.conf.default`, to compare). If PSX-DS's folder moves, it updates the line the next time it starts.

If PSX-DS is removed while this is on, the PlayStation list still works: its games start in Banana's own emulator again (since v1.4.10; before, they didn't start at all until the line was put back, and starting PSX-DS v1.4.10 once fixes a line from an older version). To tidy up, switch it off before removing PSX-DS, or delete `banana/systems.conf` if nothing else in it was changed.

## If something's wrong

Send `roms/ports/psx-ds/psx-ds.log` (and for a stutter or slowness, a log made with `export PSXDS_DEBUG=1` added near the top of `psx-ds/launch.sh`). The log's first lines say it found BananaOS, which paths it used, the window size and the frame pacing.

To compare other ways of reaching the panels, add a line near the top of `psx-ds/launch.sh` (before `if [ -d /etc/banana ]`):
- `export PSXDS_BANANA_VIDEO=evdev`: one window over both panels drawn by the CPU, copied onto them each frame (24-30 frames a second on an RG DS Plus).
- `export PSXDS_BANANA_VIDEO=offscreen`: the same drawn with OpenGL ES and read back (under 5 frames a second).
