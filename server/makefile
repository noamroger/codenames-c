# =========================
# Configuration
# =========================

CC       := gcc
SRC_DIR  := src
DIR      := build
VERSION  := $(shell cat ../VERSION 2>/dev/null || echo "0.0.0")

ifeq ($(OS),Windows_NT)
TARGET   := $(DIR)/server.exe
SOCK_LIB := -lws2_32
MKDIR_P  := if not exist "$(DIR)" mkdir "$(DIR)"
RM_RF    := if exist "$(DIR)" rmdir /S /Q "$(DIR)"
else
TARGET   := $(DIR)/server
SOCK_LIB :=
MKDIR_P  := mkdir -p $(DIR)
RM_RF    := rm -rf $(DIR)
endif

CPPFLAGS := -I./lib -DCODENAMES_VERSION=\"$(VERSION)\"

# Durcissement : protections anti-débordement de pile, contrôles de bornes sur
# les fonctions de la libc, et binaire repositionnable (ASLR complet).
HARDEN_CFLAGS  := -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
                  -Wformat -Wformat-security
HARDEN_LDFLAGS :=
ifneq ($(OS),Windows_NT)
HARDEN_CFLAGS  += -fPIE
HARDEN_LDFLAGS += -pie -Wl,-z,relro,-z,now
endif

CFLAGS   := -Wall -Wextra -O2 -MMD -MP $(HARDEN_CFLAGS)
LDFLAGS  := $(HARDEN_LDFLAGS)

# Cible de test : recompile tout sous AddressSanitizer + UBSan.
# Usage : make sanitize && ./build/server-asan -p 4242
SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1

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

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS) $(SOCK_LIB)

# Compilation générique
$(DIR)/%.o: $(SRC_DIR)/%.c | $(DIR)
	$(CC) -c $< -o $@ $(CPPFLAGS) $(CFLAGS)

# Création dossier build
$(DIR):
	$(MKDIR_P)

clean:
	$(RM_RF)

# Build instrumenté pour rejouer les tests de robustesse réseau.
sanitize: | $(DIR)
	$(CC) $(SRCS) -o $(DIR)/server-asan $(CPPFLAGS) -Wall -Wextra $(SAN_FLAGS)

-include $(DEPS)

.PHONY: all clean sanitize