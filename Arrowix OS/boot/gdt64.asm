; Arrowix OS - 64-bit Global Descriptor Table.
;
; A minimal flat GDT for long mode: a null descriptor, a 64-bit kernel code
; segment (selector 0x08), and a kernel data segment (selector 0x10). In long
; mode the segment base/limit are ignored; what matters is the code segment's
; Long (L) bit and the present/type bits.

section .boot.data
align 8

global gdt64_pointer

gdt64_base:
    dq 0                                          ; 0x00: null descriptor

gdt64_code:                                       ; 0x08: 64-bit kernel code
    ; present | descriptor type (code/data) | executable | long mode
    dq (1 << 47) | (1 << 44) | (1 << 43) | (1 << 53)

gdt64_data:                                       ; 0x10: kernel data
    ; present | descriptor type (code/data) | writable
    dq (1 << 47) | (1 << 44) | (1 << 41)

gdt64_end:

gdt64_pointer:
    dw gdt64_end - gdt64_base - 1                 ; limit
    dq gdt64_base                                 ; base
