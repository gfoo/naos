#!/usr/bin/env bash
# naos — construction du cross-compiler i686-elf (binutils + gcc)
# -----------------------------------------------------------------------------
# Pourquoi un cross-compiler ? Le gcc système produit des binaires Linux (libc,
# format hôte). Notre kernel tourne SANS OS : un toolchain i686-elf ne fait
# aucune hypothèse hôte, ce qui évite des bugs sournois (cf. docs/DESIGN-LOG.md, C5).
# Référence : https://wiki.osdev.org/GCC_Cross-Compiler
#
# B0 n'utilise PAS ce compilateur (le boot sector est en NASM pur). Il sert à
# partir de B2 (premier code C). À lancer une fois ; compte ~20-40 min.
#
# Prérequis (à installer avant, Debian/Ubuntu) :
#   sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev \
#                       libmpfr-dev texinfo wget
set -euo pipefail

BINUTILS_VERSION="${BINUTILS_VERSION:-2.43}"
GCC_VERSION="${GCC_VERSION:-14.2.0}"

export TARGET="${TARGET:-i686-elf}"
export PREFIX="${PREFIX:-$HOME/opt/cross}"   # où installer le toolchain
export PATH="$PREFIX/bin:$PATH"
SRC="${SRC:-$HOME/src/naos-toolchain}"        # où télécharger/compiler les sources
JOBS="$(nproc)"

mkdir -p "$SRC" "$PREFIX"
cd "$SRC"

echo ">> Cible=$TARGET  Préfixe=$PREFIX  Jobs=$JOBS"

# --- binutils (assembleur, linker, objcopy... pour la cible i686-elf) ---
if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
  wget -nc "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz"
  tar xf "binutils-$BINUTILS_VERSION.tar.gz"
fi
rm -rf build-binutils && mkdir build-binutils && cd build-binutils
"../binutils-$BINUTILS_VERSION/configure" --target="$TARGET" --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j"$JOBS"
make install
cd "$SRC"

# --- gcc (sans headers, langage C seulement : on n'a pas de libc) ---
if [ ! -d "gcc-$GCC_VERSION" ]; then
  wget -nc "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"
  tar xf "gcc-$GCC_VERSION.tar.gz"
fi
rm -rf build-gcc && mkdir build-gcc && cd build-gcc
"../gcc-$GCC_VERSION/configure" --target="$TARGET" --prefix="$PREFIX" \
    --disable-nls --enable-languages=c --without-headers
make -j"$JOBS" all-gcc
make -j"$JOBS" all-target-libgcc
make install-gcc
make install-target-libgcc
cd "$SRC"

echo
echo ">> Terminé. Ajoute ceci à ton ~/.bashrc (ou ~/.zshrc) :"
echo "     export PATH=\"$PREFIX/bin:\$PATH\""
echo ">> Vérifie :  $TARGET-gcc --version"
