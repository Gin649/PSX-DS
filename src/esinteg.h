/* Offering PSX-DS as an emulator for PS1 games in EmulationStation (ROCKNIX).
 *
 * ROCKNIX's EmulationStation merges es_systems_*.cfg files from its config
 * folder into its system list. Turning this on writes es_systems_psxds.cfg
 * there: the PS1 system's emulator list (copied from es_systems.cfg) plus
 * "psxds", and its launch command pointed at psxds-launch.sh, also written
 * there. That script starts PSX-DS when it's the chosen emulator and hands
 * every other launch to /usr/bin/runemu.sh unchanged. */
#ifndef PSXDS_ESINTEG_H
#define PSXDS_ESINTEG_H

#include "config.h"

#include <stdbool.h>
#include <stddef.h>

/* EmulationStation is there to add PSX-DS to (its es_systems.cfg found). */
bool es_present(const Config* cfg);
bool es_integration_on(const Config* cfg);
/* Turns it on or off; msg gets a short message for the user. */
bool es_integration_set(const Config* cfg, bool on, char* msg, size_t msgSize);
/* If it's on, rewrites the files (ROCKNIX updates can change the PS1
 * system's emulator list, and PSX-DS may have moved). */
void es_integration_refresh(const Config* cfg);

/* BananaOS (no EmulationStation): its menu reads its systems list from
 * /data/banana/systems.conf when there is one (a copy of
 * /etc/banana/systems.conf). Turning this on points the PlayStation
 * entry's run line at banana-play.sh (PSX-DS for games it has a profile for,
 * the original line for the rest), keeping the original as a comment;
 * turning it off puts it back (and deletes the copy if PSX-DS made it and
 * nothing else changed). Banana's menu reads the list when it starts. */
bool banana_present(void);
bool banana_integration_on(void);
bool banana_integration_set(bool on, char* msg, size_t msgSize);
void banana_integration_refresh(void);

#endif
