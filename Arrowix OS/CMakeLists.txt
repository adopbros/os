cmake_minimum_required(VERSION 3.21)

# Guard: Arrowix must be built with the x86_64-elf cross toolchain.
if(NOT CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "Arrowix OS must be configured with the cross toolchain file:\n"
        "  cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake")
endif()

project(ArrowixOS
    VERSION 0.1.0
    DESCRIPTION "Arrowix OS - hybrid x86_64 operating system"
    LANGUAGES C CXX ASM ASM_NASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(KernelFlags)
include(Iso)

# NASM produces 64-bit ELF objects for the boot stub and arch assembly.
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

# Default build type.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Generate compile_commands.json for tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The linker script that places the kernel in the higher half.
set(ARROWIX_KERNEL_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/kernel.ld")

# Public kernel headers, available to every kernel-space subsystem.
set(ARROWIX_KERNEL_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/kernel/include"
    # Per-module driver headers (PCI bus, RTL8139 NIC).
    "${CMAKE_SOURCE_DIR}/kernel/drivers/pci"
    "${CMAKE_SOURCE_DIR}/kernel/drivers/net/rtl8139"
)

# --- Kernel-space subsystems (object libraries) ------------------------------
# Each defines an OBJECT library that the final kernel image links together.
add_subdirectory(boot)
add_subdirectory(kernel)
add_subdirectory(drivers)
add_subdirectory(fs)

# --- Final kernel image -------------------------------------------------------
add_executable(arrowix.elf)
target_link_libraries(arrowix.elf PRIVATE
    arrowix_boot
    arrowix_kernel
    arrowix_drivers
    arrowix_fs
)
arrowix_apply_kernel_flags(arrowix.elf)
target_link_options(arrowix.elf PRIVATE
    ${ARROWIX_KERNEL_LINK_FLAGS}
    -T "${ARROWIX_KERNEL_LINKER_SCRIPT}"
    -Wl,--build-id=none
)

# libgcc provides compiler runtime helpers (e.g. 64-bit __udivdi3/__umoddi3 used
# by kprintf) that -nostdlib would otherwise drop. It must come after the object
# files in link order, so add it as a link library.
target_link_libraries(arrowix.elf PRIVATE gcc)

set_target_properties(arrowix.elf PROPERTIES
    # C++ TUs (isr.cpp, panic.cpp) make C++ the linker language so the g++ driver
    # links and can resolve -lgcc.
    LINKER_LANGUAGE CXX
    LINK_DEPENDS "${ARROWIX_KERNEL_LINKER_SCRIPT}"
)

# Bootable ISO target (requires grub-mkrescue).
arrowix_add_iso_target(arrowix.elf)

# --- User space (built with the freestanding libc; optional for now) ----------
option(ARROWIX_BUILD_USERSPACE "Build user-space components (libc, init, shell, gui)" OFF)
if(ARROWIX_BUILD_USERSPACE)
    add_subdirectory(libc)
    add_subdirectory(user)
endif()

message(STATUS "Arrowix OS ${PROJECT_VERSION} configured (build type: ${CMAKE_BUILD_TYPE})")
