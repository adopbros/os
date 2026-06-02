/*
 * Arrowix OS - Higher-half kernel linker script (x86_64).
 *
 * Layout problem: the boot stub runs with paging OFF, so its code/data must
 * live at LOW (identity) addresses; the rest of the kernel runs in the higher
 * half at 0xFFFFFFFF80000000 (-2 GiB). We solve this by placing the boot
 * sections first at physical 1 MiB (VMA == LMA == phys), then shifting the
 * location counter by KERNEL_VMA so the kernel proper is linked high while its
 * load (physical) address stays contiguous via AT().
 *
 * The boot stub builds page tables that map BOTH the low 1 GiB (identity) and
 * the higher half, then far-jumps into the high kernel (see boot/ and
 * docs/architecture/boot-flow.md).
 */

ENTRY(_start)

KERNEL_VMA = 0xFFFFFFFF80000000;   /* higher-half virtual base */

SECTIONS
{
    /* --- Low-address boot stub (paging off -> identity addresses) --------- */
    . = 1M;
    _boot_phys_start = .;

    .boot ALIGN(4K) :
    {
        /* Multiboot2 header must be in the first 32 KiB of the file. */
        KEEP(*(.multiboot))
        *(.boot.text)
        *(.boot.data)
    }

    .boot_bss ALIGN(4K) (NOLOAD) :
    {
        *(.boot.bss)
    }

    _boot_phys_end = .;

    /* --- Higher-half kernel proper --------------------------------------- */
    . += KERNEL_VMA;
    _kernel_start = .;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        _text_start = .;
        *(.text .text.*)
        _text_end = .;
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
    {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
    {
        _bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        . = ALIGN(4K);
        _bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
    }
}
