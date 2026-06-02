# Arrowix OS - Boot stub: Multiboot2 header + 32-bit entry + Long Mode transition.
# Built as an OBJECT library linked into the final kernel image.

set(BOOT_NASM_SOURCES
    multiboot2_header.asm
    boot32.asm
    paging_boot.asm
    long_mode.asm
    gdt64.asm
)

add_library(arrowix_boot OBJECT ${BOOT_NASM_SOURCES})

# NASM emits 64-bit ELF objects (object format set globally in the root
# CMakeLists via CMAKE_ASM_NASM_OBJECT_FORMAT).
