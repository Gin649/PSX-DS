/* The emulator wrapper: SwanStation (PlayStation) through libretro.
 *
 * A libretro core keeps its state in globals, so every open Emu (the game,
 * the hidden copy that opens maps) needs its own copy of the core library.
 * The copies come from emu_set_core_paths (Android: libswan_a.so and
 * libswan_b.so in the APK, each with its own soname); otherwise from
 * PSXDS_CORE, a core library copied to a temp file for each slot (glibc keys
 * loaded libraries by file, so the copies stay apart). libretro's callbacks
 * carry no context, so each slot has its own set of them.
 *
 * The picture: PS1 games draw at many sizes (256 to 640 wide, 224 to 480
 * high) and change size mid-game. The rest of the app expects one fixed
 * picture (GBA_W x GBA_H, now 640x480, 4:3), so each frame is scaled into
 * it, nearest pixel.
 *
 * What the GBA build did with the GBA's video hardware (sprite hiding, layer
 * toggles, the HUD split, VRAM and OAM access) has no PS1 equivalent: the
 * PS1 draws polygons into its own video memory. Those functions are kept so
 * the rest of the app builds, and do nothing. */
#include "emu.h"
#include "libretro.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

int emu_solar_level = 0;

#define SLOTS 4
#define AUDIO_RING (48000 * 2) /* stereo frames, a second at most */
#define MAX_OPTS 32

typedef struct Slot {
	bool used;
	void* handle;
	char path[1024];
	Emu* owner;

	void (*init)(void);
	void (*deinit)(void);
	void (*set_environment)(retro_environment_t);
	void (*set_video_refresh)(retro_video_refresh_t);
	void (*set_audio_sample)(retro_audio_sample_t);
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
	void (*set_input_poll)(retro_input_poll_t);
	void (*set_input_state)(retro_input_state_t);
	void (*set_controller_port_device)(unsigned, unsigned);
	bool (*load_game)(const struct retro_game_info*);
	void (*unload_game)(void);
	void (*run)(void);
	void (*reset)(void);
	size_t (*serialize_size)(void);
	bool (*serialize)(void*, size_t);
	bool (*unserialize)(const void*, size_t);
	void* (*get_memory_data)(unsigned);
	size_t (*get_memory_size)(unsigned);
	void (*get_system_av_info)(struct retro_system_av_info*);
	void (*cheat_reset)(void);
	void (*cheat_set)(unsigned, bool, const char*);

	unsigned pixfmt;
	uint16_t keys; /* libretro joypad bits */
	int av;        /* bit 0 video, bit 1 audio */
	unsigned rate;
	int srcW, srcH; /* the core's last picture size */

	/* sound, from the core's thread to emu_audio_read */
	pthread_mutex_t audioLock;
	int16_t* ring;
	size_t ringHead, ringCount;

	char save[1024]; /* the memory card file, written back on close */
} Slot;

static Slot slots[SLOTS];
static pthread_mutex_t slotLock = PTHREAD_MUTEX_INITIALIZER;
static char corePaths[SLOTS][1024];
static int nCorePaths;
static char sysDir[1024], saveDir[1024];
static struct { char key[96]; char value[64]; } opts[MAX_OPTS];
static int nOpts;

void emu_set_core_paths(const char* const* paths, int n) {
	nCorePaths = n < SLOTS ? n : SLOTS;
	for (int i = 0; i < nCorePaths; i++) snprintf(corePaths[i], sizeof corePaths[i], "%s", paths[i]);
}

void emu_set_dirs(const char* bios, const char* saves) {
	if (bios) snprintf(sysDir, sizeof sysDir, "%s", bios);
	if (saves) snprintf(saveDir, sizeof saveDir, "%s", saves);
}

void emu_set_option(const char* key, const char* value) {
	for (int k = 0; k < nOpts; k++)
		if (strcmp(opts[k].key, key) == 0) {
			snprintf(opts[k].value, sizeof opts[k].value, "%s", value);
			return;
		}
	if (nOpts == MAX_OPTS) return;
	snprintf(opts[nOpts].key, sizeof opts[0].key, "%s", key);
	snprintf(opts[nOpts].value, sizeof opts[0].value, "%s", value);
	nOpts++;
}

/* --- callbacks, one set per slot --------------------------------------------- */

static bool env_slot(int i, unsigned cmd, void* data);

static void log_slot(int i, enum retro_log_level level, const char* fmt, va_list ap) {
	if (level < RETRO_LOG_INFO && !getenv("GBADS_DEBUG")) return;
	char line[512];
	vsnprintf(line, sizeof line, fmt, ap);
	size_t n = strlen(line);
	while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
	fprintf(stderr, "swanstation[%d]: %s\n", i, line);
}

/* each frame scaled into the Emu's fixed GBA_W x GBA_H picture, bytes R,G,B,X */
static void video_slot(int i, const void* data, unsigned w, unsigned h, size_t pitch) {
	Slot* s = &slots[i];
	if (!data || !w || !h || !s->owner || !s->owner->fb) return; /* duped: keep the last */
	s->srcW = w;
	s->srcH = h;
	uint32_t* dst = s->owner->fb;
	unsigned xs[GBA_W];
	for (int x = 0; x < GBA_W; x++) xs[x] = (unsigned)x * w / GBA_W;
	for (int y = 0; y < GBA_H; y++) {
		const uint8_t* row = (const uint8_t*)data + (size_t)((unsigned)y * h / GBA_H) * pitch;
		uint32_t* out = dst + y * GBA_W;
		if (s->pixfmt == RETRO_PIXEL_FORMAT_XRGB8888) {
			const uint32_t* src = (const uint32_t*)row;
			for (int x = 0; x < GBA_W; x++) {
				uint32_t p = src[xs[x]];
				out[x] = 0xFF000000u | ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
			}
		} else {
			const uint16_t* src = (const uint16_t*)row;
			for (int x = 0; x < GBA_W; x++) {
				uint16_t p = src[xs[x]];
				uint32_t r, g, b;
				if (s->pixfmt == RETRO_PIXEL_FORMAT_RGB565) {
					r = (p >> 11) & 0x1F; g = (p >> 5) & 0x3F; b = p & 0x1F;
					g = (g << 2) | (g >> 4);
				} else {
					r = (p >> 10) & 0x1F; g = (p >> 5) & 0x1F; b = p & 0x1F;
					g = (g << 3) | (g >> 2);
				}
				r = (r << 3) | (r >> 2);
				b = (b << 3) | (b >> 2);
				out[x] = 0xFF000000u | (b << 16) | (g << 8) | r;
			}
		}
	}
}

static int16_t input_slot(int i, unsigned port, unsigned device, unsigned index, unsigned id) {
	(void)index;
	if (port != 0 || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD) return 0;
	uint16_t k = slots[i].keys;
	if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)k;
	return id < 16 ? (k >> id) & 1 : 0;
}

static size_t audio_batch_slot(int i, const int16_t* d, size_t frames) {
	Slot* s = &slots[i];
	if (!s->owner || !s->owner->audio || !s->ring || !(s->av & 2)) return frames;
	pthread_mutex_lock(&s->audioLock);
	for (size_t f = 0; f < frames; f++) {
		if (s->ringCount == AUDIO_RING) { /* full: drop the oldest */
			s->ringHead = (s->ringHead + 1) % AUDIO_RING;
			s->ringCount--;
		}
		size_t at = (s->ringHead + s->ringCount) % AUDIO_RING;
		s->ring[at * 2] = d[f * 2];
		s->ring[at * 2 + 1] = d[f * 2 + 1];
		s->ringCount++;
	}
	pthread_mutex_unlock(&s->audioLock);
	return frames;
}
static void audio_sample_slot(int i, int16_t l, int16_t r) {
	int16_t d[2] = { l, r };
	audio_batch_slot(i, d, 1);
}
static void poll_slot(int i) { (void)i; }

#define SLOT_CALLBACKS(N) \
	static void log_##N(enum retro_log_level l, const char* f, ...) { va_list ap; va_start(ap, f); log_slot(N, l, f, ap); va_end(ap); } \
	static bool env_##N(unsigned c, void* d) { return env_slot(N, c, d); } \
	static void video_##N(const void* d, unsigned w, unsigned h, size_t p) { video_slot(N, d, w, h, p); } \
	static int16_t input_##N(unsigned p, unsigned d, unsigned i, unsigned id) { return input_slot(N, p, d, i, id); } \
	static void audio_##N(int16_t l, int16_t r) { audio_sample_slot(N, l, r); } \
	static size_t batch_##N(const int16_t* d, size_t f) { return audio_batch_slot(N, d, f); } \
	static void poll_##N(void) { poll_slot(N); }
SLOT_CALLBACKS(0)
SLOT_CALLBACKS(1)
SLOT_CALLBACKS(2)
SLOT_CALLBACKS(3)

static const retro_log_printf_t logFns[SLOTS] = { log_0, log_1, log_2, log_3 };
static const retro_environment_t envFns[SLOTS] = { env_0, env_1, env_2, env_3 };
static const retro_video_refresh_t videoFns[SLOTS] = { video_0, video_1, video_2, video_3 };
static const retro_input_state_t inputFns[SLOTS] = { input_0, input_1, input_2, input_3 };
static const retro_audio_sample_t audioFns[SLOTS] = { audio_0, audio_1, audio_2, audio_3 };
static const retro_audio_sample_batch_t batchFns[SLOTS] = { batch_0, batch_1, batch_2, batch_3 };
static const retro_input_poll_t pollFns[SLOTS] = { poll_0, poll_1, poll_2, poll_3 };

static bool env_slot(int i, unsigned cmd, void* data) {
	Slot* s = &slots[i];
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback*)data)->log = logFns[i];
		return true;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		*(const char**)data = sysDir[0] ? sysDir : ".";
		return true;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		*(const char**)data = saveDir[0] ? saveDir : ".";
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		unsigned f = *(const unsigned*)data;
		if (f > RETRO_PIXEL_FORMAT_RGB565) return false;
		s->pixfmt = f;
		return true;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable* v = data;
		for (int k = 0; k < nOpts; k++)
			if (v->key && strcmp(v->key, opts[k].key) == 0) {
				v->value = opts[k].value;
				return true;
			}
		return false;
	}
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		*(bool*)data = false;
		return true;
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		*(bool*)data = true;
		return true;
	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
		return true;
	case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
		*(int*)data = s->av;
		return true;
	case RETRO_ENVIRONMENT_SET_HW_RENDER:
		return false; /* software renderer (for now) */
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
		const struct retro_system_av_info* av = data;
		if (av->timing.sample_rate > 0) s->rate = (unsigned)(av->timing.sample_rate + 0.5);
		return true;
	}
	case RETRO_ENVIRONMENT_SET_VARIABLES:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
	case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
	case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
	case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
	case RETRO_ENVIRONMENT_SET_MESSAGE:
	case RETRO_ENVIRONMENT_SET_GEOMETRY:
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
	case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
		return true;
	default:
		return false;
	}
}

/* --- slots ---------------------------------------------------------------------- */

/* A core library file for slot i: a given copy, or PSXDS_CORE copied */
static bool slot_file(int i, char* out, size_t n) {
	if (i < nCorePaths) {
		snprintf(out, n, "%s", corePaths[i]);
		return true;
	}
	const char* src = getenv("PSXDS_CORE");
	if (!src || !*src) return false;
	const char* tmp = getenv("TMPDIR");
	snprintf(out, n, "%s/psxds-core-%d-%d.so", tmp && *tmp ? tmp : "/tmp", (int)getpid(), i);
	struct stat st;
	if (stat(out, &st) == 0) return true;
	FILE* in = fopen(src, "rb");
	FILE* o = in ? fopen(out, "wb") : NULL;
	if (!o) {
		if (in) fclose(in);
		return false;
	}
	char buf[65536];
	size_t r;
	while ((r = fread(buf, 1, sizeof buf, in)) > 0) fwrite(buf, 1, r, o);
	fclose(in);
	fclose(o);
	return true;
}

#define SYM(name) do { \
	*(void**)&s->name = dlsym(s->handle, "retro_" #name); \
	if (!s->name) { fprintf(stderr, "core: missing retro_%s\n", #name); goto fail; } \
} while (0)

static Slot* slot_take(Emu* e) {
	pthread_mutex_lock(&slotLock);
	int i = 0;
	while (i < SLOTS && slots[i].used) i++;
	if (i == SLOTS) {
		pthread_mutex_unlock(&slotLock);
		fprintf(stderr, "core: all %d copies in use\n", SLOTS);
		return NULL;
	}
	Slot* s = &slots[i];
	memset(s, 0, sizeof *s);
	s->used = true;
	pthread_mutex_unlock(&slotLock);

	if (!slot_file(i, s->path, sizeof s->path)) {
		fprintf(stderr, "core: no library for copy %d (set PSXDS_CORE)\n", i);
		goto fail;
	}
	s->handle = dlopen(s->path, RTLD_NOW | RTLD_LOCAL);
	if (!s->handle) {
		fprintf(stderr, "core: %s\n", dlerror());
		goto fail;
	}
	for (int k = 0; k < SLOTS; k++)
		if (k != i && slots[k].used && slots[k].handle == s->handle) {
			fprintf(stderr, "core: copy %d is the same library as copy %d\n", i, k);
			s->handle = NULL;
			goto fail;
		}
	SYM(init); SYM(deinit); SYM(set_environment); SYM(set_video_refresh);
	SYM(set_audio_sample); SYM(set_audio_sample_batch); SYM(set_input_poll);
	SYM(set_input_state); SYM(set_controller_port_device); SYM(load_game);
	SYM(unload_game); SYM(run); SYM(reset); SYM(serialize_size); SYM(serialize);
	SYM(unserialize); SYM(get_memory_data); SYM(get_memory_size);
	SYM(get_system_av_info); SYM(cheat_reset); SYM(cheat_set);

	pthread_mutex_init(&s->audioLock, NULL);
	s->owner = e;
	s->av = 3;
	s->pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;
	s->set_environment(envFns[i]);
	s->init();
	s->set_video_refresh(videoFns[i]);
	s->set_audio_sample(audioFns[i]);
	s->set_audio_sample_batch(batchFns[i]);
	s->set_input_poll(pollFns[i]);
	s->set_input_state(inputFns[i]);
	return s;

fail:
	if (s->handle) dlclose(s->handle);
	s->handle = NULL;
	s->used = false;
	return NULL;
}

static void slot_release(Slot* s) {
	if (s->handle) {
		s->deinit();
		dlclose(s->handle);
	}
	free(s->ring);
	s->ring = NULL;
	s->handle = NULL;
	s->owner = NULL;
	pthread_mutex_lock(&slotLock);
	s->used = false;
	pthread_mutex_unlock(&slotLock);
}

static Slot* S(const Emu* e) { return (Slot*)e->core; }

/* --- files ------------------------------------------------------------------------- */

static const char* ext_of(const char* name) {
	const char* dot = strrchr(name, '.');
	return dot ? dot + 1 : "";
}

bool emu_is_rom_name(const char* name) {
	static const char* exts[] = { "cue", "chd", "pbp", "m3u", "iso", "img", "ccd", "ecm", "mds", "exe", "psexe", "psf", NULL };
	const char* x = ext_of(name);
	for (int i = 0; exts[i]; i++)
		if (strcasecmp(x, exts[i]) == 0) return true;
	return false;
}

bool emu_is_archive_name(const char* name) { (void)name; return false; }

/* The GBA build read the cartridge header (title at 0xA0, code at 0xAC).
 * Until PSX-DS reads a disc's serial, the header is made up: the file name
 * as the title and "PSX1" as the code. */
bool emu_rom_header(const char* path, uint8_t* out, size_t n) {
	struct stat st;
	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
	memset(out, 0, n);
	const char* base = strrchr(path, '/');
	base = base ? base + 1 : path;
	if (n > 0xA0) {
		size_t len = strcspn(base, ".");
		if (len > 12) len = 12;
		if (len > n - 0xA0) len = n - 0xA0;
		memcpy(out + 0xA0, base, len);
	}
	if (n >= 0xB0) memcpy(out + 0xAC, "PSX1", 4);
	return true;
}

/* A disc image is hundreds of MB: these read only its first MB (enough for a
 * checksum that tells files apart). */
uint8_t* emu_rom_read_all(const char* path, size_t* size) {
	FILE* f = fopen(path, "rb");
	if (!f) return NULL;
	uint8_t* buf = malloc(1 << 20);
	size_t n = buf ? fread(buf, 1, 1 << 20, f) : 0;
	fclose(f);
	if (!n) {
		free(buf);
		return NULL;
	}
	*size = n;
	return buf;
}

uint8_t* emu_rom_read_played(const char* path, size_t* size, int* patch, char* patchName, uint32_t* fileCrc) {
	uint8_t* buf = emu_rom_read_all(path, size);
	if (patch) *patch = 0;
	if (patchName) patchName[0] = 0;
	if (buf && fileCrc) *fileCrc = (uint32_t)crc32(0, buf, (uInt)*size);
	return buf;
}

void emu_patch_plan(const char* rom, char files[][1024], int n) { (void)rom; (void)files; (void)n; }

uint8_t* emu_rom_read_patched(const char* path, char files[][1024], int n, size_t* size, int* patch, char* patchName) {
	(void)files; (void)n;
	return emu_rom_read_played(path, size, patch, patchName, NULL);
}

uint32_t doCrc32(const void* buf, size_t size) { return (uint32_t)crc32(0, buf, (uInt)size); }

/* --- open / close ----------------------------------------------------------------- */

static void card_load(Slot* s, const char* path) {
	snprintf(s->save, sizeof s->save, "%s", path ? path : "");
	if (!path) return;
	uint8_t* mem = s->get_memory_data(RETRO_MEMORY_SAVE_RAM);
	size_t n = s->get_memory_size(RETRO_MEMORY_SAVE_RAM);
	FILE* f = mem && n ? fopen(path, "rb") : NULL;
	if (!f) return;
	size_t r = fread(mem, 1, n, f);
	(void)r;
	fclose(f);
}

static void card_write(Slot* s) {
	if (!s->save[0] || !s->handle) return;
	uint8_t* mem = s->get_memory_data(RETRO_MEMORY_SAVE_RAM);
	size_t n = s->get_memory_size(RETRO_MEMORY_SAVE_RAM);
	if (!mem || !n) return;
	char tmp[1100];
	snprintf(tmp, sizeof tmp, "%s.new", s->save);
	FILE* f = fopen(tmp, "wb");
	if (!f) return;
	bool ok = fwrite(mem, 1, n, f) == n;
	ok = fclose(f) == 0 && ok;
	if (ok) rename(tmp, s->save);
}

bool emu_open(Emu* e, const char* romPath, const char* savePath, bool audio) {
	memset(e, 0, sizeof *e);
	e->fb = calloc(GBA_W * GBA_H, sizeof(uint32_t));
	if (!e->fb) return false;
	Slot* s = slot_take(e);
	if (!s) {
		free(e->fb);
		e->fb = NULL;
		return false;
	}
	e->core = (struct mCore*)s;
	e->audio = audio;
	if (audio) s->ring = calloc(AUDIO_RING * 2, sizeof(int16_t));

	struct retro_game_info info = { .path = romPath };
	if (!s->load_game(&info)) {
		fprintf(stderr, "could not load %s\n", romPath);
		slot_release(s);
		free(e->fb);
		memset(e, 0, sizeof *e);
		return false;
	}
	s->set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	struct retro_system_av_info av;
	memset(&av, 0, sizeof av);
	s->get_system_av_info(&av);
	s->rate = av.timing.sample_rate > 0 ? (unsigned)(av.timing.sample_rate + 0.5) : 44100;
	card_load(s, savePath);

	uint8_t h[0xC0];
	if (emu_rom_header(romPath, h, sizeof h)) memcpy(e->gameCode, h + 0xAC, 4);
	size_t n;
	uint8_t* head = emu_rom_read_all(romPath, &n);
	if (head) {
		e->romCrc = e->fileCrc = doCrc32(head, n);
		free(head);
	}
	return true;
}

void emu_close(Emu* e) {
	Slot* s = S(e);
	if (s) {
		card_write(s);
		s->unload_game();
		slot_release(s);
	}
	free(e->fb);
	e->fb = NULL;
	e->core = NULL;
}

/* --- running ------------------------------------------------------------------------- */

/* GBA DS's keys (emu.h K_*) as libretro's PS1 pad bits */
static uint16_t pad_bits(uint32_t keys) {
	static const struct { uint32_t k; unsigned id; } map[] = {
		{ K_A, RETRO_DEVICE_ID_JOYPAD_A },          /* right: Circle */
		{ K_B, RETRO_DEVICE_ID_JOYPAD_B },          /* bottom: Cross */
		{ K_X, RETRO_DEVICE_ID_JOYPAD_X },          /* top: Triangle */
		{ K_Y, RETRO_DEVICE_ID_JOYPAD_Y },          /* left: Square */
		{ K_SELECT, RETRO_DEVICE_ID_JOYPAD_SELECT },
		{ K_START, RETRO_DEVICE_ID_JOYPAD_START },
		{ K_UP, RETRO_DEVICE_ID_JOYPAD_UP },
		{ K_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN },
		{ K_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT },
		{ K_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
		{ K_L, RETRO_DEVICE_ID_JOYPAD_L },
		{ K_R, RETRO_DEVICE_ID_JOYPAD_R },
		{ K_L2, RETRO_DEVICE_ID_JOYPAD_L2 },
		{ K_R2, RETRO_DEVICE_ID_JOYPAD_R2 },
		{ K_L3, RETRO_DEVICE_ID_JOYPAD_L3 },
		{ K_R3, RETRO_DEVICE_ID_JOYPAD_R3 },
	};
	uint16_t b = 0;
	for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
		if (keys & map[i].k) b |= 1u << map[i].id;
	return b;
}

bool emu_cancelled(const Emu* e) { return e->cancel && *e->cancel; }

static void run(Emu* e, uint32_t keys, int av) {
	Slot* s = S(e);
	if (!s || emu_cancelled(e)) return;
	s->keys = pad_bits(keys);
	s->av = av;
	s->run();
	e->frames++;
}

void emu_frame(Emu* e, uint32_t keys) { run(e, keys, e->audio ? 3 : 1); }
void emu_frame_blind(Emu* e, uint32_t keys) { run(e, keys, 0); }

void emu_reset(Emu* e) {
	if (S(e)) S(e)->reset();
}

/* --- states ----------------------------------------------------------------------------- */

size_t emu_state_size(Emu* e) { return S(e) ? S(e)->serialize_size() : 0; }
bool emu_state_save(Emu* e, void* buf) { return S(e) && S(e)->serialize(buf, S(e)->serialize_size()); }
bool emu_state_load(Emu* e, const void* buf) { return S(e) && S(e)->unserialize(buf, S(e)->serialize_size()); }

/* Rewind isn't there yet (a PS1 state is 11 MB: it needs a different design). */
void emu_rewind_init(Emu* e, size_t entries) { (void)e; (void)entries; }
void emu_rewind_deinit(void) {}
void emu_rewind_append(Emu* e) { (void)e; }
bool emu_rewind_step(Emu* e) { (void)e; return false; }
size_t emu_rewind_count(void) { return 0; }

/* State files: "PSXDSST1", the extra data's size and the game's checksum
 * (little-endian u32 each), the extra data, then the core's state. */
static const char STATE_MAGIC[8] = { 'P', 'S', 'X', 'D', 'S', 'S', 'T', '1' };

bool emu_state_save_file_as(Emu* e, const char* path, const void* extra, size_t extraN, StateFormat fmt) {
	(void)fmt;
	Slot* s = S(e);
	if (!s) return false;
	size_t n = s->serialize_size();
	void* buf = malloc(n);
	if (!buf || !s->serialize(buf, n)) {
		free(buf);
		return false;
	}
	card_write(s);
	char tmp[1100];
	snprintf(tmp, sizeof tmp, "%s.new", path);
	FILE* f = fopen(tmp, "wb");
	bool ok = f != NULL;
	if (f) {
		uint32_t hdr[2] = { (uint32_t)(extra ? extraN : 0), e->romCrc };
		ok = fwrite(STATE_MAGIC, 1, 8, f) == 8 && fwrite(hdr, 4, 2, f) == 2;
		if (ok && extra && extraN) ok = fwrite(extra, 1, extraN, f) == extraN;
		if (ok) ok = fwrite(buf, 1, n, f) == n;
		ok = fclose(f) == 0 && ok;
	}
	free(buf);
	if (ok) ok = rename(tmp, path) == 0;
	else unlink(tmp);
	return ok;
}

bool emu_state_save_file(Emu* e, const char* path) { return emu_state_save_file_as(e, path, NULL, 0, STATE_FORMAT_MGBA); }

/* reads a state file's header; returns the open file at the core's state, or NULL */
static FILE* state_open(const char* path, uint32_t* extraN, uint32_t* crc) {
	FILE* f = fopen(path, "rb");
	if (!f) return NULL;
	char magic[8];
	uint32_t hdr[2];
	if (fread(magic, 1, 8, f) != 8 || memcmp(magic, STATE_MAGIC, 8) != 0 || fread(hdr, 4, 2, f) != 2) {
		fclose(f);
		return NULL;
	}
	if (extraN) *extraN = hdr[0];
	if (crc) *crc = hdr[1];
	return f;
}

bool emu_state_load_file(Emu* e, const char* path) {
	Slot* s = S(e);
	uint32_t extraN;
	FILE* f = s ? state_open(path, &extraN, NULL) : NULL;
	if (!f) return false;
	size_t n = s->serialize_size();
	void* buf = malloc(n);
	bool ok = buf && fseek(f, extraN, SEEK_CUR) == 0 && fread(buf, 1, n, f) == n && s->unserialize(buf, n);
	fclose(f);
	free(buf);
	return ok;
}

StateFileKind emu_state_file_kind(Emu* e, const char* path, bool* fromGbads) {
	(void)e;
	if (fromGbads) *fromGbads = false;
	struct stat st;
	if (stat(path, &st) != 0) return STATE_FILE_NONE;
	uint32_t extraN;
	FILE* f = state_open(path, &extraN, NULL);
	if (!f) return STATE_FILE_OTHER_CORE;
	fclose(f);
	if (fromGbads) *fromGbads = extraN > 0;
	return STATE_FILE_MGBA;
}

bool emu_state_file_rom_crc(const char* path, uint32_t* crc) {
	FILE* f = state_open(path, NULL, crc);
	if (!f) return false;
	fclose(f);
	return true;
}

void* emu_state_file_get_extra(Emu* e, const char* path, size_t* n) {
	(void)e;
	uint32_t extraN;
	FILE* f = state_open(path, &extraN, NULL);
	if (!f) return NULL;
	void* buf = extraN ? malloc(extraN) : NULL;
	if (buf && fread(buf, 1, extraN, f) != extraN) {
		free(buf);
		buf = NULL;
	}
	fclose(f);
	if (buf) *n = extraN;
	return buf;
}

/* --- memory -------------------------------------------------------------------------- */

/* region: 0 the PS1's main RAM (2 MB), 0xE the memory card */
uint8_t* emu_memory(Emu* e, int region, size_t* size) {
	Slot* s = S(e);
	if (!s) return NULL;
	unsigned id = region == 0xE ? RETRO_MEMORY_SAVE_RAM : RETRO_MEMORY_SYSTEM_RAM;
	if (region != 0 && region != 0xE) return NULL;
	*size = s->get_memory_size(id);
	return s->get_memory_data(id);
}

/* PS1 addresses: main RAM through any of its mirrors (0x00..., 0x80...,
 * 0xA0...); anything else reads 0 */
static uint8_t* ram_at(Emu* e, uint32_t addr, size_t len) {
	if ((addr & 0x1F000000u) != 0) return NULL;
	size_t n;
	uint8_t* ram = emu_memory(e, 0, &n);
	uint32_t off = addr & 0x1FFFFFu;
	if (!ram || off + len > n) return NULL;
	return ram + off;
}

uint8_t emu_read8(Emu* e, uint32_t addr) { uint8_t* p = ram_at(e, addr, 1); return p ? *p : 0; }
uint16_t emu_read16(Emu* e, uint32_t addr) { uint8_t* p = ram_at(e, addr, 2); uint16_t v = 0; if (p) memcpy(&v, p, 2); return v; }
uint32_t emu_read32(Emu* e, uint32_t addr) { uint8_t* p = ram_at(e, addr, 4); uint32_t v = 0; if (p) memcpy(&v, p, 4); return v; }
void emu_write8(Emu* e, uint32_t addr, uint8_t value) { uint8_t* p = ram_at(e, addr, 1); if (p) *p = value; }
void emu_write16(Emu* e, uint32_t addr, uint16_t value) { uint8_t* p = ram_at(e, addr, 2); if (p) memcpy(p, &value, 2); }
void emu_write32(Emu* e, uint32_t addr, uint32_t value) { uint8_t* p = ram_at(e, addr, 4); if (p) memcpy(p, &value, 4); }

/* --- sound ------------------------------------------------------------------------------ */

size_t emu_audio_read(Emu* e, int16_t* out, size_t maxFrames) {
	Slot* s = S(e);
	if (!s || !s->ring) return 0;
	pthread_mutex_lock(&s->audioLock);
	size_t n = s->ringCount < maxFrames ? s->ringCount : maxFrames;
	for (size_t f = 0; f < n; f++) {
		size_t at = (s->ringHead + f) % AUDIO_RING;
		out[f * 2] = s->ring[at * 2];
		out[f * 2 + 1] = s->ring[at * 2 + 1];
	}
	s->ringHead = (s->ringHead + n) % AUDIO_RING;
	s->ringCount -= n;
	pthread_mutex_unlock(&s->audioLock);
	return n;
}

unsigned emu_audio_rate(Emu* e) { return S(e) ? S(e)->rate : 44100; }

/* --- cheats ----------------------------------------------------------------------------- */

static int cheatCount;

void emu_cheats_clear(Emu* e) {
	if (S(e)) S(e)->cheat_reset();
	cheatCount = 0;
}

/* codes in RetroArch's .cht form ("XXXXXXXX YYYY" GameShark lines joined with
 * '+'); SwanStation takes them as they are */
int emu_cheats_add(Emu* e, const char* name, const char* const* codes, int n, int type) {
	(void)name; (void)type;
	Slot* s = S(e);
	if (!s) return 0;
	int added = 0;
	for (int i = 0; i < n; i++)
		if (codes[i] && *codes[i]) {
			s->cheat_set(cheatCount++, true, codes[i]);
			added++;
		}
	return added;
}

/* --- the GBA's video hardware: nothing on the PS1 ------------------------------------ */

void emu_hide_sprites(Emu* e, const SpriteRule* r, int n) { (void)e; (void)r; (void)n; }
void emu_keep_sprites(Emu* e, const SpriteRule* r, int n) { (void)e; (void)r; (void)n; }
void emu_hide_grouped(Emu* e, bool on) { (void)e; (void)on; }
void emu_hide_exact(Emu* e, const uint16_t (*a)[3], int n) { (void)e; (void)a; (void)n; }
void emu_layers(Emu* e, unsigned mask) { (void)e; (void)mask; }
bool emu_hud_split(Emu* e, bool on, unsigned bg, const SpriteRule* o, int n, uint32_t* hud) { (void)e; (void)on; (void)bg; (void)o; (void)n; (void)hud; return false; }
uint32_t emu_hud_clear(void) { return 0xFF00FF; }
void emu_hud_grab(Emu* e, unsigned bg, uint32_t* buf) { (void)e; (void)bg; (void)buf; }
void emu_hud_grab_cnt(Emu* e, uint16_t cnt, uint32_t* buf) { (void)e; (void)cnt; (void)buf; }
void emu_hud_by_priority(int p) { (void)p; }
void emu_hud_keep_bgs(unsigned m) { (void)m; }
void emu_hud_mem_layers(const uint32_t a[2], const uint32_t m[2], const uint32_t v[2], const int s[2], int n, bool any, unsigned l, int gp) { (void)a; (void)m; (void)v; (void)s; (void)n; (void)any; (void)l; (void)gp; }
void emu_hud_game_reg(uint32_t a, uint16_t b) { (void)a; (void)b; }
void emu_hud_band(bool on, bool copy, bool watch) { (void)on; (void)copy; (void)watch; }
void emu_hud_copy(Emu* e, bool copy) { (void)e; (void)copy; }
void emu_hud_copy_above(int row) { (void)row; }
void emu_hud_rows(int rows) { (void)rows; }
void emu_hud_keep_exact(const uint16_t (*a)[3], int n) { (void)a; (void)n; }
bool emu_hud_band_rows(int* y0, int* y1) { (void)y0; (void)y1; return false; }
bool emu_hud_grabbed(void) { return false; }
uint16_t emu_io16(Emu* e, uint32_t off) { (void)e; (void)off; return 0; }
static uint16_t noVram[0xC000];
const uint16_t* emu_vram(Emu* e) { (void)e; return noVram; }
void emu_vram_write(Emu* e, uint32_t off, const void* src, size_t n) { (void)e; (void)off; (void)src; (void)n; }
void emu_oam_read(Emu* e, uint16_t* out) { (void)e; memset(out, 0, 1024); }
void emu_oam_write(Emu* e, const uint16_t* src) { (void)e; (void)src; }
