#!/usr/bin/env bash
# Arrowix OS - Build the x86_64-elf cross-compiler (binutils + GCC).
#
# This produces a freestanding cross toolchain (no host libc) suitable for
# building a bare-metal kernel. Run under Linux or WSL2. See toolchain/README.md.
#
# Override versions/paths via environment:
#   BINUTILS_VERSION, GCC_VERSION, PREFIX, JOBS
set -euo pipefail

BINUTILS_VERSION="${BINUTILS_VERSION:-2.42}"
GCC_VERSION="${GCC_VERSION:-14.2.0}"
TARGET="x86_64-elf"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$ROOT/cross}"
SRC="$ROOT/src"
BUILD="$ROOT/build"
JOBS="${JOBS:-$(nproc)}"

export PATH="$PREFIX/bin:$PATH"

echo ">> Cross toolchain target=$TARGET prefix=$PREFIX"
echo ">> binutils=$BINUTILS_VERSION gcc=$GCC_VERSION jobs=$JOBS"

mkdir -p "$SRC" "$BUILD" "$PREFIX"

# --- Fetch sources -----------------------------------------------------------
fetch() {
    local url="$1" out="$2"
    [[ -f "$SRC/$out" ]] || curl -L --fail -o "$SRC/$out" "$url"
}

fetch "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz" \
      "binutils-$BINUTILS_VERSION.tar.xz"
fetch "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz" \
      "gcc-$GCC_VERSION.tar.xz"

tar -C "$SRC" -xf "$SRC/binutils-$BINUTILS_VERSION.tar.xz"
tar -C "$SRC" -xf "$SRC/gcc-$GCC_VERSION.tar.xz"

# GCC prerequisites (gmp, mpfr, mpc, isl).
( cd "$SRC/gcc-$GCC_VERSION" && ./contrib/download_prerequisites )

# --- Build binutils ----------------------------------------------------------
mkdir -p "$BUILD/binutils"
( cd "$BUILD/binutils"
  "$SRC/binutils-$BINUTILS_VERSION/configure" \
      --target="$TARGET" --prefix="$PREFIX" \
      --with-sysroot --disable-nls --disable-werror
  make -j"$JOBS"
  make install )

# --- Build GCC (C and C++, freestanding, no headers) -------------------------
mkdir -p "$BUILD/gcc"
( cd "$BUILD/gcc"
  "$SRC/gcc-$GCC_VERSION/configure" \
      --target="$TARGET" --prefix="$PREFIX" \
      --disable-nls --enable-languages=c,c++ --without-headers \
      --disable-hosted-libstdcxx
  make -j"$JOBS" all-gcc
  make -j"$JOBS" all-target-libgcc
  make install-gcc
  make install-target-libgcc )

echo ">> Done. Add to PATH:  export PATH=\"$PREFIX/bin:\$PATH\""
echo ">> Verify:            $TARGET-gcc --version"
