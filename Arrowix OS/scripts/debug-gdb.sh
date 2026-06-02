#!/usr/bin/env bash
# Arrowix OS - Attach GDB to a QEMU instance started with --debug.
#
# Run `scripts/run-qemu.sh --debug` in one terminal, then this in another.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
KERNEL_ELF="${KERNEL_ELF:-$BUILD_DIR/arrowix.elf}"

GDB_BIN="gdb"
command -v x86_64-elf-gdb >/dev/null 2>&1 && GDB_BIN="x86_64-elf-gdb"

exec "$GDB_BIN" "$KERNEL_ELF" \
    -ex "set architecture i386:x86-64" \
    -ex "target remote localhost:1234" \
    -ex "break kmain" \
    -ex "continue"
