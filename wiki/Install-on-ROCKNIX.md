PSX-DS runs on stock [ROCKNIX](https://rocknix.org) and on ROCKNIXDS, on the Anbernic RG DS Plus (two 1024×768 screens) and the RG DS (two 640×480 screens).

## Install

1. Download the latest `psx-ds-v…-linux-aarch64.zip` from the [Releases page](https://github.com/Gin649/PSX-DS/releases).
2. Unzip it into `/storage/roms/ports/` (over the network share, or with a file manager). You get:
   - `psx-ds/`: the app, its settings (`psx-ds.cfg`) and, later, its saved data (`states/`);
   - `PSX-DS.sh`: the **PSX-DS** entry in EmulationStation's Ports;
   - `images/PSX-DS.png`: the entry's icon (see [the Ports icon](#the-ports-icon) below).
3. Put your ROMs in `/storage/roms/psx/` (subfolders are fine):
   - `.gba` files, or zipped as `.zip` or `.7z` (the first PS1 ROM in the archive is used);
   - saves and states are named after the file, so `Game.zip` and an unzipped `Game.gba` share them.
4. In EmulationStation, update the game lists (Start → Game settings → Update gamelists, or restart EmulationStation) and launch **PSX-DS** from **Ports**.

The first time the game list opens, PSX-DS pulls each game's logo out of the ROMs in the background. The selected game and the ones on screen go first, and you can start a game as soon as its own logo is ready; the rest carry on while you're in the game list. Tiles say "Loading thumbnail..." until their logo is ready. See [[Playing]] for the game list itself.

## The two screens on stock ROCKNIX

Stock ROCKNIX keeps the RG DS Plus's bottom screen switched off and ties the touchscreen to the top one. While PSX-DS runs, `psx-ds/screens.sh` switches the bottom screen on, places it next to the top one, and maps the touchscreen across both; when PSX-DS closes (or is killed), it puts everything back as it was. Nothing to set up; `psx-ds.log` records the screen layout before and after. ROCKNIXDS already has both screens on, and works the same way.

## The Ports icon

The zip includes an icon for the Ports entry: **`/storage/roms/ports/images/PSX-DS.png`**, a 1024×1024 PNG (its source is [`assets/brand/psx-ds-icon.png`](https://github.com/Gin649/PSX-DS/blob/main/assets/brand/psx-ds-icon.png) in the repository). EmulationStation doesn't pick it up by itself, and scrapers won't find PSX-DS in their databases, so point the entry at it once:

**From EmulationStation (easiest):**
1. Go to **Ports** and select **PSX-DS**.
2. Open the game options (usually **SELECT**) → **Edit this game's metadata**.
3. Set **Image** to `/storage/roms/ports/images/PSX-DS.png` (browse to `images/` in the Ports folder).
4. Save. Some themes also show **Thumbnail** or **Marquee**; the same file works for those.

**By editing `gamelist.xml`:** EmulationStation keeps the Ports entries in `/storage/roms/ports/gamelist.xml`. Edit it while EmulationStation isn't running (it rewrites the file when it exits, which would undo your change), e.g. over SSH after stopping it, or from another computer over the network share. Add an entry like this inside `<gameList>` (or add the `<image>` line to the existing PSX-DS entry):

```xml
<game>
	<path>./PSX-DS.sh</path>
	<name>PSX-DS</name>
	<desc>PlayStation with a second screen: maps, HUDs and logos on the other screen.</desc>
	<image>./images/PSX-DS.png</image>
	<thumbnail>./images/PSX-DS.png</thumbnail>
</game>
```

Paths are relative to the Ports folder. Upgrading PSX-DS replaces the image file but not your `gamelist.xml`, so the entry keeps its icon.

## Starting games from EmulationStation's PlayStation list

PSX-DS can also be one of the emulators for the **PlayStation** system, so you can start a game from EmulationStation's own PS1 list instead of opening PSX-DS first.

1. Open PSX-DS's game list and go to **Settings** (tap it, top right, or press **SELECT**).
2. Switch **PSX-DS in EmulationStation** on.
3. Restart EmulationStation (Start → Quit → Restart EmulationStation).
4. Pick PSX-DS:
   - for one game: select it, open its game options (usually **SELECT**) → **Advanced game options** → **Emulator** → **PSXDS**;
   - for every PS1 game: in the PlayStation system's advanced settings, set the emulator to **PSXDS**.

Closing PSX-DS goes straight back to EmulationStation. Your other PS1 emulators keep working as before.

**How it works:** the setting writes two files into `/storage/.config/emulationstation/` (`es_config_dir` in `psx-ds.cfg` changes the folder):
- `es_systems_psxds.cfg`, which EmulationStation merges into its system list: a copy of the PS1 system's emulator list with PSX-DS added, and the PS1 launch command pointed at `psxds-launch.sh`;
- `psxds-launch.sh`, which starts PSX-DS when it's the chosen emulator, run by ROCKNIX's launcher as a Port (so your settings for Ports apply), and hands every other launch to `/usr/bin/runemu.sh` exactly as before.

PSX-DS rewrites the copied emulator list each time it starts, so it keeps up with ROCKNIX updates. Switching the setting off deletes both files. If you delete the PSX-DS folder without switching it off first, games set to PSX-DS fall back to SwanStation; delete the two files to tidy up.

## Upgrading

The easiest way: in the game list, **Settings** > **Updates** > **Check now**, then **Update now** (PSX-DS also offers new versions by itself once a day; see [[Playing#updates]]). That keeps your `psx-ds.cfg` too.

By hand: unzip the new release over the old one, into `/storage/roms/ports/`.

- **Kept:** everything in `psx-ds/states/` (save states, resume points, cheat choices, menu settings, extracted logos, your RetroAchievements sign-in), your own `logos/`, `overlays/`, `cheats/` and `patches/`, and the battery saves next to your ROMs. If you keep PSX-DS's files in a folder of your own (`data-folder.cfg`, below), that file and that folder are kept too.
- **Replaced** (by hand only): `psx-ds/psx-ds.cfg`. If you've edited it, keep a copy first and copy your changes back (new versions may add options; see [[Settings]]).
- If **PSX-DS in EmulationStation** was on, it stays on.

**From gbadual** (this app's old name): unzip as above, then launch **PSX-DS** once. On that first run it copies your save states, resume points, menu settings and any custom logos or overlays from the old `gbadual/` folder. Then delete the old `gbadual/` folder and the **PS1 Dual Screen** entry (`PS1 Dual Screen.sh`) from `/storage/roms/ports/`. Copy any changes you'd made in `gbadual.cfg` into `psx-ds/psx-ds.cfg`.

## Keeping PSX-DS's files somewhere else

By default PSX-DS keeps its own files (`states/`, `cheats/`, `overlays/`, `logos/`, `patches/`, `bug-reports/`) in its folder, `psx-ds/`, beside the program. To keep them elsewhere, make a file called `data-folder.cfg` in `psx-ds/` with one line:

```
data_dir = /storage/roms/ports-data/psx-ds
```

The next time PSX-DS starts, it moves its files there (the program, `psx-ds.cfg` and `psx-ds.log` stay in `psx-ds/`). If that folder already has PSX-DS's files in it, they're used as they are and nothing is moved. Take the line out to move them back. Updates (and unzipping a new version over the old one) leave `data-folder.cfg` alone, so the choice stays. If the folder can't be reached (its card isn't in), the game list says so and PSX-DS uses `psx-ds/` for that run. The same works on BananaOS and other systems.

## Uninstalling

1. If you switched **PSX-DS in EmulationStation** on, switch it off first (game list → Settings).
2. Delete `psx-ds/`, `PSX-DS.sh` and `images/PSX-DS.png` from `/storage/roms/ports/`. If you moved PSX-DS's files with `data-folder.cfg`, delete that folder too.

Battery saves (`.sav`, next to your ROMs) stay; other emulators can use them.
