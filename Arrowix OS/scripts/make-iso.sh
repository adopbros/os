#!/usr/bin/env bash
# Arrowix OS - Build a bootable GRUB ISO from the compiled kernel ELF.
#
# Prefers the CMake 'iso' target (which knows the build dir layout); falls back
# to staging an isodir manually. Requires grub-mkrescue + xorriso.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
KERNEL_ELF="${KERNEL_ELF:-$BUILD_DIR/arrowix.elf}"
ISO_OUT="${ISO_OUT:-$BUILD_DIR/arrowix.iso}"

if [[ ! -f "$KERNEL_ELF" ]]; then
    echo "error: kernel not found at $KERNEL_ELF" >&2
    echo "build it first: cmake --build \"$BUILD_DIR\"" >&2
    exit 1
fi

if ! command -v grub-mkrescue >/dev/null 2>&1; then
    echo "error: grub-mkrescue not found (install grub + xorriso)" >&2
    exit 1
fi

ISODIR="$BUILD_DIR/isodir"
rm -rf "$ISODIR"
mkdir -p "$ISODIR/boot/grub"
cp "$KERNEL_ELF" "$ISODIR/boot/arrowix.elf"
cp "$ROOT/boot/grub/grub.cfg" "$ISODIR/boot/grub/grub.cfg"

grub-mkrescue -o "$ISO_OUT" "$ISODIR"
echo "ISO created: $ISO_OUT"
