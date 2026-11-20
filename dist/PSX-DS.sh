#!/bin/bash
# Ports entry for PSX-DS (ROCKNIX, BananaOS, the RG DS Plus's Stock Linux OS). Opens the game library, which
# lists every .gba in the PS1 ROM folder (and its subfolders) each time it starts.
# launch.sh is run with bash, not started directly, so it works even when
# the files lost their permissions on the way (unzipped on a computer and
# copied over a network share, say).
D="$(cd "$(dirname "$0")" && pwd)"
if [ ! -f "$D/psx-ds/launch.sh" ]; then
	echo "PSX-DS: $D/psx-ds/launch.sh is missing: unzip the whole zip into this folder" > "$D/psx-ds-error.log"
	exit 1
fi
exec bash "$D/psx-ds/launch.sh" "$@"
