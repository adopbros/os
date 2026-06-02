# Arrowix OS

Arrowix OS is an ambitious, modular, 64-bit (x86_64) operating system built on a
**hybrid kernel** architecture. It boots via **GRUB / Multiboot2**, transitions the
CPU from 32-bit Protected Mode into 64-bit **Long Mode**, and is organized as a
monorepo of cleanly separated subsystems.

> Status: Bootstrapping (Phase 0 / Phase 1). See [PLAN.md](PLAN.md) for the full roadmap.

## Architecture at a glance

- **Target:** `x86_64` (AMD64), hybrid kernel.
- **Boot:** GRUB 2 + Multiboot2 -> 32-bit stub -> 4-level paging -> Long Mode -> higher-half kernel.
- **Languages:** C17 + freestanding C++ (no RTTI, no exceptions) + NASM assembly.
- **Build:** CMake with a dedicated `x86_64-elf` cross-compiler toolchain file.
- **Kernel layout:** higher-half at `0xFFFFFFFF80000000` (`-mcmodel=kernel`).

## Repository layout

| Path         | Purpose |
|--------------|---------|
| `boot/`      | Multiboot2 header, 32-bit entry, Long Mode transition, boot page tables. |
| `kernel/`    | Core kernel: arch/x86_64, memory management, scheduler, interrupts, syscalls. |
| `drivers/`   | Device drivers: VGA/VESA video, PS/2 keyboard, storage, serial, timers, PCI. |
| `fs/`        | Filesystems: VFS layer, ramfs/initrd, FAT32. |
| `libc/`      | Minimal freestanding C standard library (user) and `libk` subset (kernel). |
| `user/`      | User space: init (PID 1), shell, apps, and the GUI stack. |
| `linker/`    | Linker scripts (`kernel.ld`, `user.ld`). |
| `cmake/`     | Toolchain file and shared CMake modules / flags. |
| `toolchain/` | Scripts to build the `x86_64-elf` cross-compiler. |
| `scripts/`   | QEMU run, ISO creation (`grub-mkrescue`), GDB debugging. |
| `docs/`      | Architecture documentation and ADRs. |
| `tests/`     | Unit tests. |

## Prerequisites

The host of record is Windows, but the toolchain (cross-GCC, GRUB tools, QEMU) is
easiest on Linux. **WSL2 (Ubuntu)** or MSYS2 is recommended.

Required tooling:

- `x86_64-elf-gcc` and `x86_64-elf-binutils` (built via `toolchain/build-cross-gcc.sh`).
- `nasm`
- `cmake` (>= 3.21) and `ninja` or `make`
- `grub-mkrescue` + `xorriso` (for ISO generation)
- `qemu-system-x86_64`
- `gdb` (preferably `x86_64-elf-gdb`) for debugging

## Quick start

```bash
# 1) Build the cross-compiler once (long; see toolchain/README.md)
./toolchain/build-cross-gcc.sh

# 2) Configure + build with the cross toolchain
cmake -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
cmake --build build

# 3) Create a bootable ISO and run it in QEMU
./scripts/make-iso.sh
./scripts/run-qemu.sh
```

On Windows/PowerShell, use the `.ps1` wrappers in `scripts/` (which delegate to WSL).

## License

See `LICENSE` (to be added).
