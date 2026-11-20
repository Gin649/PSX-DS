#!/bin/bash
# Started by ROCKNIX's runemu.sh (as a Port) for a PS1 game picked in
# EmulationStation with PSX-DS as its emulator; see psxds-launch.sh in
# EmulationStation's config folder. The game comes in PSXDS_ROM.
exec bash "$(dirname "$0")/launch.sh" "$PSXDS_ROM"
