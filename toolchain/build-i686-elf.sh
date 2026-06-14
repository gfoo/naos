#!/usr/bin/env bash
# naos — building the i686-elf cross-compiler (binutils + gcc)
# -----------------------------------------------------------------------------
# Why a cross-compiler? The system gcc produces Linux binaries (libc,
# host format). Our kernel runs with NO OS: an i686-elf toolchain makes
# no host assumptions, which avoids sneaky bugs (see docs/DESIGN-LOG.md, C5).
# Reference: https://wiki.osdev.org/GCC_Cross-Compiler
#
# B0 does NOT use this compiler (the boot sector is pure NASM). It is used
# starting from B2 (first C code). Run it once; expect ~20-40 min.
#
# Prerequisites (install beforehand, Debian/Ubuntu):
#   sudo apt install -y build-essential bison flex libgmp-dev libmpc-dev \
#                       libmpfr-dev texinfo wget
set -euo pipefail

BINUTILS_VERSION="${BINUTILS_VERSION:-2.43}"
GCC_VERSION="${GCC_VERSION:-14.2.0}"

export TARGET="${TARGET:-i686-elf}"
export PREFIX="${PREFIX:-$HOME/opt/cross}"   # where to install the toolchain
export PATH="$PREFIX/bin:$PATH"
SRC="${SRC:-$HOME/src/naos-toolchain}"        # where to download/compile the sources
JOBS="$(nproc)"

mkdir -p "$SRC" "$PREFIX"
cd "$SRC"

echo ">> Target=$TARGET  Prefix=$PREFIX  Jobs=$JOBS"

# --- binutils (assembler, linker, objcopy... for the i686-elf target) ---
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

# --- gcc (without headers, C language only: we have no libc) ---
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
echo ">> Done. Add this to your ~/.bashrc (or ~/.zshrc):"
echo "     export PATH=\"$PREFIX/bin:\$PATH\""
echo ">> Check:  $TARGET-gcc --version"
