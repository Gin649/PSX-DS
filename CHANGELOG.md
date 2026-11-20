# Changelog

## v0.1.0 (not yet released)

- PSX-DS: a PlayStation fork of GBA DS by kyannon. SwanStation replaces mGBA, and everything specific to Game Boy Advance games (their second-screen profiles, logos, dump lists, patches list, tests and tools) is removed.
- PS1 disc images (.cue, .chd, .pbp, .m3u and others) show in the game list; save states, memory cards, cheats and sound work.
- PS1 buttons: Triangle, Square, L2, R2, L3 and R3 as well as the usual ones.

Known issues:
- No game has its second screen yet; the second screen shows nothing for now.
- Every game has the same code until PSX-DS reads each disc's ID.
- Rewind is off (a PS1 state is 11 MB: it needs a different design).
- Some settings and help pages still describe GBA DS features (layers, LCD shaders, ROM patches) that don't apply to PS1 games yet.

GBA DS's own history: https://github.com/kyannon/gba-ds/blob/main/CHANGELOG.md
