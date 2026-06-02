#!/usr/bin/env bash
# Arrowix OS - Boot the ISO in QEMU.
#
# Serial is redirected to stdio so early kernel output (COM1) is visible in the
# terminal. Pass --debug to wait for a GDB connection on tcp::1234.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
ISO="${ISO:-$BUILD_DIR/arrowix.iso}"

QEMU_ARGS=(
    -cdrom "$ISO"
    -m 512M
    -serial stdio
    -no-reboot
    -d guest_errors
)

if [[ "${1:-}" == "--debug" ]]; then
    echo "QEMU paused for GDB on tcp::1234 (use scripts/debug-gdb.sh)"
    QEMU_ARGS+=( -S -gdb tcp::1234 )
fi

if [[ ! -f "$ISO" ]]; then
    echo "error: ISO not found at $ISO; run scripts/make-iso.sh first" >&2
    exit 1
fi

exec qemu-system-x86_64 "${QEMU_ARGS[@]}"
