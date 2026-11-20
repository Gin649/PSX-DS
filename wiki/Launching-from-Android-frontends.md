On Android, launchers ("frontends") can start PSX-DS straight into a game. PSX-DS then skips its game list, and **Close PSX-DS** in the pause menu goes back to the launcher. Starting another game from the launcher while PSX-DS is open saves the current game's resume point and switches.

All launchers do the same thing underneath: they send Android an *intent* naming PSX-DS's launch activity and the game. This page gives the setup for the common ones, then the details for anything else.

| | |
|---|---|
| Package | `io.github.gin649.psxds` |
| Activity | `io.github.gin649.psxds.LauncherActivity` (short form `.LauncherActivity`) |
| Action | `android.intent.action.VIEW` (or none) |
| Game | the intent's data (`file://` or `content://` URI), **or** an extra named `rom` (a path or URI) |

## GammaOS's launcher

GammaOS (the RG DS Plus's Android) has its own game launcher. In **Game Systems** → **PlayStation** (or a new system you make for PSX-DS):

| Field | Value |
|---|---|
| Launch Type | **Custom Package** |
| Package | `io.github.gin649.psxds` |
| Intent Template | `-n io.github.gin649.psxds/.LauncherActivity -a android.intent.action.VIEW -d {file.uri}` |
| Launch Args | leave empty |

Use this exact Intent Template. GammaOS's default template only opens the app without saying which game: PSX-DS then opens on a black screen or its game list, and `PSX-DS/launch.log` shows `action=null data=null`.

To keep another emulator as the default for PS1 and use PSX-DS only for some games, make a second system for PSX-DS pointing at the same ROM folder, or set the emulator per game if your GammaOS version offers it.

## Daijisho

In Daijisho, open the **PlayStation** platform's settings and add a player (the exact menu names vary between Daijisho versions):

| Field | Value |
|---|---|
| Name | `PSX-DS` |
| Package | `io.github.gin649.psxds` |
| Activity | `.LauncherActivity` |
| Arguments | `-a android.intent.action.VIEW -d {file.uri}` |

If that doesn't start the game (some Daijisho versions or storage setups give a URI PSX-DS can't read), pass the path instead: `-e rom {file.path}`.

Daijisho uses the same template language as GammaOS, so the GammaOS template above also works there as a whole (`-n …/.LauncherActivity -a … -d {file.uri}`).

## ES-DE

In PSX-DS's game list, open **Settings** and switch on **PSX-DS in ES-DE**. Then quit ES-DE and open it again: PlayStation games start in PSX-DS.

ES-DE keeps its other emulators in the list. For a game PSX-DS doesn't support, choose another one in ES-DE: open the game's metadata editor, then **Alternative emulator**.

The row is there when PSX-DS finds ES-DE's folder (`ES-DE` on the internal storage or an SD card). It puts PSX-DS into two files in `ES-DE/custom_systems`:

- `es_find_rules.xml`: where to find PSX-DS.
- `es_systems.xml`: **PSX-DS (Standalone)** first in the `gba` system's list of emulators. If you already have your own `gba` system there, PSX-DS is added to it. If you don't, PSX-DS adds a copy of ES-DE's own `gba` system.

Switching it off takes out only what PSX-DS added, and deletes either file if nothing else is left in it.

### By hand

To set it up yourself instead, put this in `ES-DE/custom_systems/es_find_rules.xml` (create it if needed, with `<ruleList>` around the entry):

```xml
<emulator name="PSX-DS">
    <!-- PlayStation, dual screen: PSX-DS -->
    <rule type="androidpackage">
        <entry>io.github.gin649.psxds/.LauncherActivity</entry>
    </rule>
</emulator>
```

In `ES-DE/custom_systems/es_systems.xml`, copy the `gba` system from ES-DE's bundled `es_systems.xml` and add this `<command>` line to it. The first `<command>` is the default emulator:

```xml
<command label="PSX-DS (Standalone)">%EMULATOR_PSX-DS% %ACTIVITY_CLEAR_TASK% %ACTIVITY_CLEAR_TOP% %EXTRA_rom%=%ROM%</command>
```

This passes the game's path, so patches next to the game work. (With `%DATA%=%ROMSAF%` instead, they don't.)

## Cocoon

[Cocoon](https://cocoon-shell.com/) picks each platform's emulator from a list of players that comes with the app (its platform files, in Daijisho's format). PSX-DS's entry has been offered to Cocoon for its PlayStation platform ([CocoonFE pull request #561](https://github.com/inssekt/CocoonFE/pull/561)). Once Cocoon lists it, choose it in Cocoon's Settings: scroll to **PlayStation**, then **Default Player** → **PSX-DS**, and save.

The entry starts PSX-DS the same way as the other launchers here:

| Field | Value |
|---|---|
| Name | `PSX-DS` |
| Arguments | `-n io.github.gin649.psxds/.LauncherActivity -a android.intent.action.VIEW -d {file.uri} --activity-clear-task --activity-clear-top` |
| Files | `.gba`, `.zip`, `.7z` |

## Any other launcher

PSX-DS accepts the game in any of these ways (it checks them in this order):

1. **An extra** with the game's path, or a `file://` / `content://` URI as text or as a URI. Accepted names: `rom` (preferred), `ROM`, `Rom`, `path`, `PATH`, `file`, `FILE`, `romPath`, `GamePath`, `bootPath`, `BootPath`, `uri`, `URI`, `romUri`, `filePath`, `file_path`, `rom_path`, `gamePath`, `game`.
2. **The intent's data**: a `file://` or `content://` URI, with or without a MIME type, usually with the `android.intent.action.VIEW` action.
3. **ClipData**: the first item's URI (some launchers attach the file this way).

A `content://` URI from Android's own storage provider (e.g. `content://com.android.externalstorage.documents/…`) is turned back into a plain path, which works with All files access. A URI from another app's provider is copied into the app's own storage first (`Android/data/io.github.gin649.psxds/files/launched`), which takes a moment for a large ROM, and saves then go next to the copy; patches aren't applied to a game started this way. Prefer paths or `file://` URIs where the launcher offers them.

**Testing with adb:**

```sh
# a path in an extra
adb shell am start -n io.github.gin649.psxds/.LauncherActivity --es rom "/storage/emulated/0/ROMs/gba/Metroid Fusion (USA).gba"

# a URI as the data, the way GammaOS and Daijisho send it
adb shell am start -n io.github.gin649.psxds/.LauncherActivity -a android.intent.action.VIEW \
    -d "file:///storage/emulated/0/ROMs/gba/Metroid%20Fusion%20(USA).gba"
```

## When a launch doesn't work

PSX-DS writes every launch request to **`PSX-DS/launch.log`**: the action, data, MIME type, flags, every extra and any ClipData it received, then the game it worked out from them (or that it found none), and any crash in the launch step. Open it after a failed launch:

- `action=null data=null` and no extras: the launcher didn't say which game. Set its template or arguments as above.
- a `content://…` data URI followed by an error: the launcher's storage provider didn't let PSX-DS read the file. Use a path extra (`-e rom {file.path}`) instead.
- the game was found but nothing appears: see `PSX-DS/psx-ds.log` and [[Troubleshooting]].

Starting games from launchers was added in v1.1 and confirmed with GammaOS's launcher (with the template above). The other launchers' setups follow their documentation and the formats PSX-DS accepts; reports of what works are welcome in the [issues](https://github.com/Gin649/PSX-DS/issues).
