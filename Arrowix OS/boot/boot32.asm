; Arrowix OS - 32-bit boot entry (Protected Mode -> long mode setup).
;
; GRUB hands control here in 32-bit protected mode (paging off) with:
;   EAX = 0x36d76289 (Multiboot2 magic), EBX = pointer to Multiboot2 info.
; We validate, verify long-mode support, build initial page tables, enable
; long mode, then far-jump into 64-bit code (see long_mode.asm).

bits 32

MB2_BOOT_MAGIC equ 0x36d76289

extern setup_page_tables          ; paging_boot.asm
extern enable_long_mode_paging    ; long_mode.asm
extern long_mode_trampoline       ; long_mode.asm (64-bit)
extern gdt64_pointer              ; gdt64.asm

global _start
global mb_magic
global mb_info

section .boot.text
_start:
    mov esp, boot_stack_top       ; set up a temporary stack
    mov [mb_magic], eax           ; stash Multiboot2 magic + info pointer
    mov [mb_info], ebx

    call check_multiboot
    call check_cpuid
    call check_long_mode

    call setup_page_tables        ; build PML4/PDPT/PD (identity + higher half)
    call enable_long_mode_paging  ; CR4.PAE -> EFER.LME -> CR0.PG

    lgdt [gdt64_pointer]          ; load 64-bit GDT
    jmp 0x08:long_mode_trampoline ; far jump -> 64-bit code segment

; --- Validate the Multiboot2 magic GRUB passed in EAX -----------------------
check_multiboot:
    cmp dword [mb_magic], MB2_BOOT_MAGIC
    jne .fail
    ret
.fail:
    mov al, 'M'
    jmp boot_error

; --- Verify CPUID is available by toggling EFLAGS.ID (bit 21) ---------------
check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx              ; restore original EFLAGS
    popfd
    cmp eax, ecx
    je .fail
    ret
.fail:
    mov al, 'C'
    jmp boot_error

; --- Verify long mode via extended CPUID (0x80000001, EDX bit 29) ----------
check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .fail
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29     ; LM bit
    jz .fail
    ret
.fail:
    mov al, 'L'
    jmp boot_error

; --- Print "ERR: X" to VGA text buffer (0xB8000) and halt -------------------
; AL = error code character. White-on-red attribute (0x4F).
boot_error:
    mov dword [0xB8000], 0x4F524F45   ; "ER"
    mov dword [0xB8004], 0x4F3A4F52   ; "R:"
    mov dword [0xB8008], 0x4F004F20   ; " " + slot for code
    mov byte  [0xB800A], al
    mov byte  [0xB800B], 0x4F
.hang:
    hlt
    jmp .hang

section .boot.bss
align 16
mb_magic: resd 1
mb_info:  resd 1

; Temporary boot stack (16 KiB).
align 16
boot_stack_bottom:
    resb 16384
boot_stack_top:
