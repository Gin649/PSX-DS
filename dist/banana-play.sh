#!/bin/bash
# BananaOS: its PlayStation list runs this for every game once PSX-DS is
# switched on there (PSX-DS's game list > Settings > PSX-DS in BananaOS's PS1
# list, which edits /data/banana/systems.conf). Games PSX-DS has second-screen
# support for start in PSX-DS; every other game goes to the list's own
# emulator, the run line kept in systems.conf as "# psx-ds: run = ...".
#   banana-play.sh <rom>
DIR="$(cd "$(dirname "$0")" && pwd)"
ROM="$1"
CONF=/data/banana/systems.conf
if "$DIR/psx-ds" --supported "$ROM" >/dev/null 2>&1; then
	# PSX-DS runs two copies of the game (the second for the map): all the
	# clock the profile for ports gives
	banana-perf performance >/dev/null 2>&1
	exec bash "$DIR/launch.sh" "$ROM"
fi
ORIG=$(sed -n 's/^# psx-ds: run = //p' "$CONF" 2>/dev/null | head -n 1)
[ -n "$ORIG" ] || ORIG='/usr/libexec/banana/run-retroarch gpsp %ROM%'
export PSXDS_ROM="$ROM"
ORIG=${ORIG//%ROM%/\"\$PSXDS_ROM\"}
ORIG=${ORIG//%SYSTEM%/gba}
exec sh -c "$ORIG"
