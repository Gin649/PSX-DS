#include "profiles.h"


#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* PSX-DS: the game profiles. Each describes how a PS1 game's map or menu is
 * brought up on the hidden copy and how to tell it is showing (see
 * profiles.h). None yet: PS1 games are added one at a time. */
static const MapProfile profiles[] = {
	{ .codes = { NULL } }, /* (the end of the list) */
};

typedef struct {
	const char* code;
	const char* base;
	int memShift;
	void (*patch)(MapProfile*);
} Variant;

/* Releases that share another's profile with changes (none yet). */
static const Variant variants[] = { { "----", "----", 0, NULL } };
#define N_VARIANTS (sizeof(variants) / sizeof(*variants))
static MapProfile variantProfiles[N_VARIANTS];
static pthread_once_t variantsOnce = PTHREAD_ONCE_INIT;

static const MapProfile* base_find(const char* gameCode) {
	for (size_t i = 0; i < sizeof(profiles) / sizeof(*profiles); ++i) {
		for (int j = 0; j < 4 && profiles[i].codes[j]; ++j) {
			if (!strncmp(profiles[i].codes[j], gameCode, 4)) {
				return &profiles[i];
			}
		}
	}
	return NULL;
}

static void shift(MemProbe* list, int n, int by) {
	for (int i = 0; i < n && list[i].addr; ++i) list[i].addr += by;
}

static void build_variants(void) {
	for (size_t i = 0; i < N_VARIANTS; ++i) {
		const Variant* v = &variants[i];
		MapProfile* p = &variantProfiles[i];
		const MapProfile* b = base_find(v->base);
		if (!b) continue;
		*p = *b;
		memset(p->codes, 0, sizeof(p->codes));
		p->codes[0] = v->code;
		shift(p->mem, 4, v->memShift);
		shift(p->live, 4, v->memShift);
		shift(p->menu, 2, v->memShift);
		shift(p->room, 4, v->memShift);
		shift(p->open2If, 2, v->memShift);
		if (v->patch) v->patch(p);
	}
}

bool profile_ram_moved(const char* gameCode) {
	for (size_t i = 0; i < N_VARIANTS; ++i) {
		if (!strncmp(variants[i].code, gameCode, 4)) return variants[i].memShift != 0;
	}
	return false;
}

/* ---------------------------------------------------------- colour patches
 * A known patch's result (docs/patches.md) that changes colours the profile
 * looks for (the map screen's probes, the lettering map_crop erases...):
 * those colours, swapped for the ones the patched game draws. Found by
 * opening the screens on the clean and the patched ROM and pairing their
 * pixels. The game being played says which ROM it is (profile_set_rom). */
typedef struct {
	char code[5];
	uint32_t crc; /* the patched ROM */
	int n;
	uint32_t from[8], to[8];
} ColorSwap;
static const ColorSwap colorSwaps[] = { { "----", 0, 0, { 0 }, { 0 } } };
static const ColorSwap* romSwap; /* the game being played's, or NULL */
static MapProfile swapped;        /* its profile with the colours swapped */

static void swap_colour(uint32_t* c) {
	for (int i = 0; romSwap && i < romSwap->n; ++i) {
		if (*c == romSwap->from[i]) {
			*c = romSwap->to[i];
			return;
		}
	}
}

static const MapProfile* find_unswapped(const char* gameCode);

void profile_set_rom(const char* gameCode, uint32_t crc) {
	romSwap = NULL;
	for (size_t i = 0; gameCode && i < sizeof(colorSwaps) / sizeof(colorSwaps[0]); ++i)
		if (!strncmp(colorSwaps[i].code, gameCode, 4) && colorSwaps[i].crc == crc) romSwap = &colorSwaps[i];
	const MapProfile* p = romSwap ? find_unswapped(gameCode) : NULL;
	if (!p) {
		romSwap = NULL;
		return;
	}
	swapped = *p;
	for (size_t i = 0; i < sizeof(swapped.probes) / sizeof(swapped.probes[0]); ++i) swap_colour(&swapped.probes[i].rgb);
	for (size_t i = 0; i < sizeof(swapped.glyphs) / sizeof(swapped.glyphs[0]); ++i)
		for (int k = 0; k < 8; ++k) swap_colour(&swapped.glyphs[i].rgb[k]);
	for (size_t i = 0; i < sizeof(swapped.recolor) / sizeof(swapped.recolor[0]); ++i)
		for (int k = 0; k < 16; ++k) swap_colour(&swapped.recolor[i].from[k]), swap_colour(&swapped.recolor[i].to[k]);
	for (int k = 0; k < 12; ++k) swap_colour(&swapped.tab.colors[k]);
	swap_colour(&swapped.bgColor);
	swap_colour(&swapped.rate.text);
	fprintf(stderr, "profile: colours swapped for the patched ROM %08x (%d)\n", crc, romSwap->n);
}

const MapProfile* profile_find(const char* gameCode) {
	if (romSwap && !strncmp(romSwap->code, gameCode, 4)) return &swapped;
	return find_unswapped(gameCode);
}

static const MapProfile* find_unswapped(const char* gameCode) {
	const MapProfile* p = base_find(gameCode);
	if (p) return p;
	pthread_once(&variantsOnce, build_variants);
	for (size_t i = 0; i < N_VARIANTS; ++i) {
		if (!strncmp(variants[i].code, gameCode, 4) && variantProfiles[i].codes[0]) return &variantProfiles[i];
	}
	return NULL;
}

bool profile_patch_rom(const MapProfile* p, Emu* e) {
	if (!p || !p->romPatch[0].addr) return false;
	for (int pass = 0; pass < 2; ++pass) {
		for (const RomPatch* r = p->romPatch; r < p->romPatch + 3 && r->addr; ++r) {
			for (int i = 0; i < r->len; ++i) {
				if (pass == 0 && emu_read8(e, r->addr + i) != (r->orig ? r->orig[i] : 0xFF)) return false;
				if (pass == 1) emu_write8(e, r->addr + i, r->bytes[i]);
			}
		}
	}
	return true;
}

/* ---------------------------------------------------------- known dumps */
typedef struct {
	char code[5];
	uint32_t crc;
	bool tested;
	bool mod; /* a romhack or patch (a Mods row), not one of No-Intro's dumps */
	const char* name;
} KnownDump;
#include "dumps_data.h"

DumpKind profile_dump(const char* gameCode, uint32_t crc, const char** name) {
	bool listed = false;
	if (name) *name = NULL;
	for (size_t i = 0; i < sizeof(knownDumps) / sizeof(knownDumps[0]); ++i) {
		if (strncmp(knownDumps[i].code, gameCode, 4)) continue;
		if (!knownDumps[i].mod) listed = true; /* (only patches known for it: not checked) */
		if (knownDumps[i].crc != crc) continue;
		if (name) *name = knownDumps[i].name;
		if (knownDumps[i].mod) return knownDumps[i].tested ? DUMP_MOD_TESTED : DUMP_MOD;
		return knownDumps[i].tested ? DUMP_TESTED : DUMP_OFFICIAL;
	}
	return listed ? DUMP_MODIFIED : DUMP_UNLISTED;
}

uint32_t doCrc32(const void* buf, size_t size); /* (emu.c) */
const KnownPatch* profile_patch(const uint8_t* file, size_t size, int n) {
	if (size <= 4) return NULL;
	uint32_t id = doCrc32(file, size - 4);
	for (size_t i = 0; i < sizeof(knownPatches) / sizeof(knownPatches[0]); ++i) {
		if (knownPatches[i].id != id || knownPatches[i].size != size) continue;
		if (n-- == 0) return &knownPatches[i];
	}
	return NULL;
}

/* The release's name as No-Intro gives it ("Mario & Luigi - Superstar Saga
 * (USA)"): its tested dump's, else its first official one's, without what
 * sets that dump apart ("(Rev 1)", "(Virtual Console)"...): the region and
 * languages kept. NULL for a release with no list of dumps. (GEN-9's
 * patches folder.) */
bool profile_release_name(const char* gameCode, char* out, size_t size) {
	const char* name = NULL;
	for (size_t i = 0; i < sizeof(knownDumps) / sizeof(knownDumps[0]); ++i) {
		if (strncmp(knownDumps[i].code, gameCode, 4) || knownDumps[i].mod) continue;
		if (!name || knownDumps[i].tested) name = knownDumps[i].name;
		if (knownDumps[i].tested) break;
	}
	if (!name) return false;
	/* the title, then "(Region)", then "(En,Fr,...)" if it's languages */
	const char* p = strchr(name, '(');
	size_t keep = p ? (size_t) (p - name) : strlen(name);
	while (keep && name[keep - 1] == ' ') --keep;
	const char* e = p ? strchr(p, ')') : NULL;
	if (e) {
		keep = (size_t) (e + 1 - name); /* the region */
		if (e[1] == ' ' && e[2] == '(') {
			const char* p2 = e + 2;
			const char* e2 = strchr(p2, ')');
			bool langs = e2 && e2 > p2 + 1;
			for (const char* q = p2 + 1; langs && q < e2; ++q) {
				if (!((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || *q == ',')) langs = false;
			}
			if (langs) keep = (size_t) (e2 + 1 - name);
		}
	}
	snprintf(out, size, "%.*s", (int) keep, name);
	return true;
}
