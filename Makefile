# PSX-DS build
#
#   scripts/setup-host.sh     first time: fetches and builds SwanStation and SDL2 into deps/, then builds
#   make / make tools         host build (build/host/psx-ds, build/host/harness)
#   make CROSS=aarch64-linux-gnu- SDL_PREFIX=../sdl-aarch64
#
# ZLIB       how to link zlib (default -lz; ZLIB_INC its include flags)
# SDL_PREFIX an SDL2 install prefix (include/SDL2, lib/libSDL2.so)

CROSS      ?=
CC         := $(CROSS)gcc
SDL_PREFIX ?= deps/sdl-host
OUT        ?= build/$(if $(CROSS),$(patsubst %-,%,$(CROSS)),host)

VERSION    := $(shell cat VERSION 2>/dev/null || echo dev)
# SIDE=name: a side build, installed beside the main one to try a branch's
# work (scripts/build-rocknix.sh PSXDS_SIDE): its version is VERSION+name,
# with no updates and no EmulationStation / BananaOS integration
# SIDE_CODES: the game codes its game list shows (space-separated; the
# others aren't listed: only the game being worked on and its releases)
SIDE       ?=
SIDE_CODES ?=
ifneq ($(SIDE),)
VERSION    := $(VERSION)+$(SIDE)
SIDE_DEF   := -DPSXDS_SIDE='"$(SIDE)"'
ifneq ($(SIDE_CODES),)
SIDE_DEF   += -DPSXDS_SIDE_CODES='"$(SIDE_CODES)"'
endif
endif
OPTFLAGS   ?= -O2 -g

override CFLAGS := $(OPTFLAGS) -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
              -DPSXDS_VERSION='"$(VERSION)"' $(SIDE_DEF) -I$(SDL_PREFIX)/include/SDL2 $(ZLIB_INC) -D_REENTRANT
LDFLAGS    += -L$(SDL_PREFIX)/lib -Wl,-rpath-link,$(SDL_PREFIX)/lib
# the PlayStation core (SwanStation) is loaded at run time (emu.c), not linked
ZLIB       ?= -lz
LIBS       := -lSDL2 $(ZLIB) -ldl -lpthread -lm

CORE_SRC   := src/emu.c src/patch.c src/mapcap.c src/profiles.c src/logo_masks.c
# RetroAchievements: rcheevos (MIT), vendored in third_party/rcheevos
RC_DIR     := third_party/rcheevos
RC_SRC     := $(wildcard $(RC_DIR)/src/*.c $(RC_DIR)/src/rapi/*.c $(RC_DIR)/src/rcheevos/*.c) $(RC_DIR)/src/rhash/md5.c
RC_OBJ     := $(patsubst $(RC_DIR)/src/%.c,$(OUT)/rcheevos/%.o,$(RC_SRC))
# WebP logos: libwebp's decoder (BSD), vendored in third_party/libwebp
WEBP_DIR   := third_party/libwebp
WEBP_SRC   := $(wildcard $(WEBP_DIR)/src/dec/*.c $(WEBP_DIR)/src/dsp/*.c $(WEBP_DIR)/src/utils/*.c)
WEBP_OBJ   := $(patsubst $(WEBP_DIR)/src/%.c,$(OUT)/libwebp/%.o,$(WEBP_SRC))

APP_SRC    := src/main.c src/config.c src/i18n.c src/i18n_data.c src/font.c src/ui.c src/library.c src/mapper.c src/hotkeys.c src/folderpick.c src/game.c src/paths.c src/image.c src/brand_data.c src/bezel.c src/bezel_logo.c src/logo.c src/cheats.c src/esinteg.c src/textfont.c src/fonts_data.c src/ra.c src/http.c src/sha256.c src/update.c src/unzip.c src/clocks.c $(CORE_SRC)

all: $(OUT)/psx-ds

$(OUT)/psx-ds: $(APP_SRC) src/*.h VERSION $(RC_OBJ) $(WEBP_OBJ)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -DPSXDS_RA -I$(RC_DIR)/include -I$(RC_DIR)/src -I$(WEBP_DIR) -o $@ $(APP_SRC) $(RC_OBJ) $(WEBP_OBJ) $(LDFLAGS) $(LIBS)

$(OUT)/rcheevos/%.o: $(RC_DIR)/src/%.c $(wildcard $(RC_DIR)/include/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(OPTFLAGS) -std=gnu11 -I$(RC_DIR)/include -c $< -o $@

$(OUT)/libwebp/%.o: $(WEBP_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(OPTFLAGS) -std=gnu11 -I$(WEBP_DIR) -c $< -o $@

# development tools (host only; need libpng)
tools: $(OUT)/harness

$(OUT)/harness: tools/harness.c tools/png.c $(CORE_SRC) src/logo.c src/*.h
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ tools/harness.c tools/png.c $(CORE_SRC) src/logo.c $(ZLIB) -lpng -ldl -lpthread -lm

clean:
	rm -rf build

.PHONY: all tools clean

# tests for the interface's language and its fonts (tools/test_i18n.c); DejaVu Sans is
# only the stand-in fallback face for the font part
test-i18n: tools/test_i18n.c src/i18n.c src/textfont.c src/fonts_data.c
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/test-i18n $^ -lm -lpthread
	$(OUT)/test-i18n $(firstword $(wildcard /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf))
