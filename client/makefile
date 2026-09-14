# =========================
# Configuration
# =========================

CC       := gcc
SRC_DIR  := src
DIR      := build
VERSION  := $(shell cat ../VERSION 2>/dev/null || echo "0.0.0")

ifeq ($(OS),Windows_NT)
TARGET   := $(DIR)/client.exe
EXTRA_NET_LIBS := -lws2_32
WINDRES  := $(shell command -v windres 2>/dev/null || command -v x86_64-w64-mingw32-windres 2>/dev/null)
RC_FILE  := client_icon.rc
RC_OBJ   := $(DIR)/client_icon.res
ifeq ($(strip $(WINDRES)),)
$(warning windres not found; building without embedded executable icon)
EXTRA_RES_OBJS :=
else
EXTRA_RES_OBJS := $(RC_OBJ)
endif
RPATH_FLAGS :=
MKDIR_P  := if not exist "$(DIR)" mkdir "$(DIR)"
RM_RF    := if exist "$(DIR)" rmdir /S /Q "$(DIR)"
else
TARGET   := $(DIR)/client
EXTRA_NET_LIBS :=
EXTRA_RES_OBJS :=
RPATH_FLAGS := -Wl,-rpath,'$$ORIGIN/../SDL2/lib'
MKDIR_P  := mkdir -p $(DIR)
RM_RF    := rm -rf $(DIR)
endif

SDL_CFLAGS_BASE := $(shell pkg-config --cflags sdl2 2>/dev/null || ./SDL2/bin/sdl2-config --cflags 2>/dev/null || echo "-I./SDL2/include")
SDL_CFLAGS := $(SDL_CFLAGS_BASE) -I./SDL2/include

SDL_LDFLAGS_BASE := $(shell pkg-config --libs sdl2 2>/dev/null || ./SDL2/bin/sdl2-config --libs 2>/dev/null || echo "-L./SDL2/lib -lSDL2")
# Always append image/ttf/mixer libs (they may not be provided by sdl2-config)
SDL_LDFLAGS := $(SDL_LDFLAGS_BASE) -L./SDL2/lib -lSDL2_image -lSDL2_ttf -lSDL2_mixer

CPPFLAGS := -D_GNU_SOURCE -I./lib $(SDL_CFLAGS) -DCODENAMES_VERSION=\"$(VERSION)\"

# Durcissement : le client parse des données venues du réseau, ces protections
# transforment un éventuel débordement en arrêt net plutôt qu'en exécution de code.
HARDEN_CFLAGS  := -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
                  -Wformat -Wformat-security
HARDEN_LDFLAGS :=
ifneq ($(OS),Windows_NT)
HARDEN_CFLAGS  += -fPIE
HARDEN_LDFLAGS += -pie -Wl,-z,relro,-z,now
endif

CFLAGS   := -Wall -Wextra -O2 -MMD -MP -finput-charset=UTF-8 $(HARDEN_CFLAGS)
LDFLAGS  := $(SDL_LDFLAGS) -lm $(RPATH_FLAGS) $(EXTRA_NET_LIBS) $(HARDEN_LDFLAGS)

# =========================
# Sources
# =========================

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# =========================
# Rules
# =========================

all: $(TARGET)

$(TARGET): $(OBJS) $(EXTRA_RES_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compilation générique
$(DIR)/%.o: $(SRC_DIR)/%.c | $(DIR)
	$(CC) -c $< -o $@ $(CPPFLAGS) $(CFLAGS)

ifneq ($(strip $(EXTRA_RES_OBJS)),)
$(RC_OBJ): $(RC_FILE) | $(DIR)
	$(WINDRES) $< -O coff -o $@
endif

# Création dossier build si nécessaire
$(DIR):
	$(MKDIR_P)

clean:
	$(RM_RF)

-include $(DEPS)

.PHONY: all clean