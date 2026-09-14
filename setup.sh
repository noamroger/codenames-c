#!/usr/bin/env bash
set -e

# =============================
# Configuration
# =============================
SDL_VERSION=2.30.0
SDL_IMAGE_VERSION=2.8.2
SDL_TTF_VERSION=2.22.0
SDL_MIXER_VERSION=2.8.0

# Empreintes SHA256 officielles des archives (libsdl-org).
# Elles sont vérifiées avant extraction : sans ce contrôle, un miroir ou une
# résolution DNS compromis permettrait d'injecter du code à la compilation.
SDL_SHA256=36e2e41557e0fa4a1519315c0f5958a87ccb27e25c51776beb6f1239526447b0
SDL_IMAGE_SHA256=8f486bbfbcf8464dd58c9e5d93394ab0255ce68b51c5a966a918244820a76ddc
SDL_TTF_SHA256=d48cbd1ce475b9e178206bf3b72d56b66d84d44f64ac05803328396234d67723
SDL_MIXER_SHA256=1cfb34c87b26dbdbc7afd68c4f545c0116ab5f90bbfecc5aebe2a9cb4bb31549

ROOT_DIR="$(pwd)"
DEPS_DIR="$ROOT_DIR/client"
BUILD_DIR="$DEPS_DIR/build_SDL2"
INSTALL_DIR="$DEPS_DIR/SDL2"

# =============================
# Préparation des dossiers
# =============================
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"
cd "$BUILD_DIR"

# =============================
# Fonction de téléchargement
# =============================
verify_checksum () {
    # $1 = fichier, $2 = empreinte SHA256 attendue
    actual=$(sha256sum "$1" | cut -d' ' -f1)
    if [ "$actual" != "$2" ]; then
        echo "ERREUR : empreinte invalide pour $1" >&2
        echo "  attendue : $2" >&2
        echo "  obtenue  : $actual" >&2
        echo "L'archive a été modifiée ou le téléchargement est corrompu. Abandon." >&2
        rm -f "$1"
        exit 1
    fi
    echo "  empreinte SHA256 vérifiée pour $1"
}

download () {
    # $1 = fichier, $2 = URL, $3 = empreinte SHA256 attendue
    if [ ! -f "$1" ]; then
        echo "Téléchargement de $1..."
        wget "$2"
    fi
    verify_checksum "$1" "$3"
}

# =============================
# Téléchargement
# =============================
download SDL2-$SDL_VERSION.tar.gz \
https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz \
"$SDL_SHA256"

download SDL2_image-$SDL_IMAGE_VERSION.tar.gz \
https://github.com/libsdl-org/SDL_image/releases/download/release-$SDL_IMAGE_VERSION/SDL2_image-$SDL_IMAGE_VERSION.tar.gz \
"$SDL_IMAGE_SHA256"

download SDL2_ttf-$SDL_TTF_VERSION.tar.gz \
https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL_TTF_VERSION/SDL2_ttf-$SDL_TTF_VERSION.tar.gz \
"$SDL_TTF_SHA256"

download SDL2_mixer-$SDL_MIXER_VERSION.tar.gz \
https://github.com/libsdl-org/SDL_mixer/releases/download/release-$SDL_MIXER_VERSION/SDL2_mixer-$SDL_MIXER_VERSION.tar.gz \
"$SDL_MIXER_SHA256"

# =============================
# Extraction
# =============================
tar xzf SDL2-$SDL_VERSION.tar.gz
tar xzf SDL2_image-$SDL_IMAGE_VERSION.tar.gz
tar xzf SDL2_ttf-$SDL_TTF_VERSION.tar.gz
tar xzf SDL2_mixer-$SDL_MIXER_VERSION.tar.gz

# =============================
# Compilation SDL2
# =============================
cd SDL2-$SDL_VERSION
./configure --prefix="$INSTALL_DIR"
make -j$(nproc)
make install
cd ..

# =============================
# Compilation SDL2_image
# =============================
cd SDL2_image-$SDL_IMAGE_VERSION
./configure \
  --prefix="$INSTALL_DIR" \
  --with-sdl-prefix="$INSTALL_DIR"
make -j$(nproc)
make install
cd ..

# =============================
# Compilation SDL2_ttf
# =============================
cd SDL2_ttf-$SDL_TTF_VERSION
./configure \
  --prefix="$INSTALL_DIR" \
  --with-sdl-prefix="$INSTALL_DIR"
make -j$(nproc)
make install
cd ..

# =============================
# Compilation SDL2_mixer
# =============================
cd SDL2_mixer-$SDL_MIXER_VERSION
./configure \
  --prefix="$INSTALL_DIR" \
  --with-sdl-prefix="$INSTALL_DIR"
make -j$(nproc)
make install
cd ..

rm -rf "$BUILD_DIR"

if git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git -C "$ROOT_DIR" config core.hooksPath .githooks
  echo "Git hooks configured: core.hooksPath=.githooks"
fi

echo
echo "====================================================================="
echo " SDL2 + SDL2_image + SDL2_ttf + SDL2_mixer installés avec succès "
echo "====================================================================="