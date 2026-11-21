# Changelog

## v0.1.0 (not yet released)

- PSX-DS: a PlayStation fork of GBA DS by kyannon. SwanStation replaces mGBA, and everything specific to Game Boy Advance games (their second-screen profiles, logos, dump lists, patches list, tests and tools) is removed.
- PS1 disc images (.cue, .chd, .pbp, .m3u and others) show in the game list; save states, memory cards, cheats and sound work.
- Controls for a PlayStation pad: every button reaches the game by position (Circle right, Cross bottom, Triangle top, Square left, L1/R1, L2/R2, L3/R3), each one remappable in Settings > Controls.
- A DualShock with both analog sticks (Settings > Controls > Controller): analog on from the start, or switched with L1 + R1 + L3 + R3 for games that don't like it, or the first digital pad.
- Menus: Cross selects and Circle goes back (Confirm button: Circle, for the Japanese way). Button prompts and menu text show PlayStation buttons.
- Fast forward and rewind: the hotkey button with R2 / L2, so L2 and R2 alone are the game's. The left stick no longer works as the D-pad unless that's switched on.

Known issues:
- No game has its second screen yet; the second screen shows nothing for now.
- Every game has the same code until PSX-DS reads each disc's ID.
- Rewind is off (a PS1 state is 11 MB: it needs a different design).
- Some settings and help pages still describe GBA DS features (layers, LCD shaders, ROM patches) that don't apply to PS1 games yet.

GBA DS's own history: https://github.com/kyannon/gba-ds/blob/main/CHANGELOG.md
