# Arrowix OS - CMake toolchain file for the x86_64-elf bare-metal cross-compiler.
#
# Usage:
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
#
# Assumes x86_64-elf-gcc / x86_64-elf-g++ / x86_64-elf-* are on PATH (built via
# toolchain/build-cross-gcc.sh). Override the prefix with -DARROWIX_CROSS_PREFIX=...

# We are building for a bare-metal target with no operating system.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compiler prefix (default: x86_64-elf-). Allow override and optional path.
if(NOT DEFINED ARROWIX_CROSS_PREFIX)
    set(ARROWIX_CROSS_PREFIX "x86_64-elf-")
endif()

# Optional directory holding the cross toolchain binaries.
if(DEFINED ARROWIX_CROSS_DIR)
    set(_cross_path "${ARROWIX_CROSS_DIR}/")
else()
    set(_cross_path "")
endif()

set(CMAKE_C_COMPILER   "${_cross_path}${ARROWIX_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${_cross_path}${ARROWIX_CROSS_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${_cross_path}${ARROWIX_CROSS_PREFIX}gcc")
set(CMAKE_AR           "${_cross_path}${ARROWIX_CROSS_PREFIX}ar")
set(CMAKE_RANLIB       "${_cross_path}${ARROWIX_CROSS_PREFIX}ranlib")
set(CMAKE_OBJCOPY      "${_cross_path}${ARROWIX_CROSS_PREFIX}objcopy")
set(CMAKE_OBJDUMP      "${_cross_path}${ARROWIX_CROSS_PREFIX}objdump")
set(CMAKE_STRIP        "${_cross_path}${ARROWIX_CROSS_PREFIX}strip")

# NASM is used for the boot stub and arch assembly that prefers Intel syntax.
# (GAS via gcc handles .S files; NASM handles .asm files - see boot/CMakeLists.txt.)
find_program(CMAKE_ASM_NASM_COMPILER NAMES nasm)

# The cross-compiler cannot link a hosted executable during compiler detection,
# so tell CMake to test it by building a static library instead.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Do not look for host programs/libraries/headers via the toolchain sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
