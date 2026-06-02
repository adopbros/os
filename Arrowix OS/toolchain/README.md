# Arrowix OS - Cross Toolchain

Arrowix is compiled with a dedicated `x86_64-elf` **cross-compiler** so the build
never depends on the host C library or runtime (freestanding).

## Why a cross-compiler?

A normal `gcc` targets the host OS and links its libc/crt by default. For a kernel
we need a compiler that targets bare metal (`x86_64-elf`): no libc, no crt0, no
host assumptions. This avoids subtle ABI/headers contamination.

## Building it

Run under **Linux or WSL2** (recommended on Windows):

```bash
# Optional: pin versions
export BINUTILS_VERSION=2.42 GCC_VERSION=14.2.0
./build-cross-gcc.sh
export PATH="$PWD/cross/bin:$PATH"
x86_64-elf-gcc --version
```

Dependencies (Debian/Ubuntu):

```bash
sudo apt update
sudo apt install -y build-essential bison flex libgmp3-dev libmpc-dev \
                    libmpfr-dev texinfo libisl-dev curl xz-utils \
                    nasm qemu-system-x86 grub-pc-bin grub-common xorriso
```

## Using it with CMake

```bash
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
cmake --build build
```

If the cross binaries are not on PATH, point CMake at them:

```bash
cmake -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake \
      -DARROWIX_CROSS_DIR=/abs/path/to/toolchain/cross/bin
```

## Notes

- Prebuilt `x86_64-elf` toolchains exist (e.g. via Homebrew on macOS or some
  package repos); any equivalent works as long as the binaries are named
  `x86_64-elf-*`.
- The build artifacts (`src/`, `build/`, `cross/`) are git-ignored.
