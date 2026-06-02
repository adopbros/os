; Arrowix OS - Multiboot2 header.
;
; Placed in the .multiboot section, which the linker puts at the very start of
; the image (within the first 32 KiB) so GRUB can find it. GRUB reads this,
; loads our ELF, and enters our kernel in 32-bit protected mode with paging off.

MB2_MAGIC        equ 0xE85250D6        ; Multiboot2 magic
MB2_ARCH         equ 0                 ; 0 = i386 (32-bit protected mode)

section .multiboot
align 8
mb2_header_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd mb2_header_end - mb2_header_start                                   ; header length
    dd 0x100000000 - (MB2_MAGIC + MB2_ARCH + (mb2_header_end - mb2_header_start)) ; checksum

    ; --- End tag (type 0, size 8) ---
    align 8
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
mb2_header_end:
