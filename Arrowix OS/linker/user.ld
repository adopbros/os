/*
 * Arrowix OS - User-space program linker script (x86_64, Ring 3).
 *
 * User programs are loaded in the lower half of the address space. This is a
 * placeholder used from Phase 7 onward (ELF loader + user space). The base may
 * be randomized later (ASLR).
 */

ENTRY(_start)

USER_BASE = 0x0000000000400000;   /* 4 MiB: conventional low-half load base */

SECTIONS
{
    . = USER_BASE;

    .text ALIGN(4K) : { *(.text .text.*) }
    .rodata ALIGN(4K) : { *(.rodata .rodata.*) }
    .data ALIGN(4K) : { *(.data .data.*) }
    .bss ALIGN(4K) : { *(COMMON) *(.bss .bss.*) }

    /DISCARD/ : { *(.eh_frame) *(.comment) *(.note .note.*) }
}
