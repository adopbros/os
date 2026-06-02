; Arrowix OS - Enable long mode + 64-bit trampoline into the higher-half kernel.
;
; enable_long_mode_paging (32-bit): performs the exact enable sequence
;   CR3 <- PML4 ; CR4.PAE=1 ; EFER.LME=1 (and NXE) ; CR0.PG=1
; which puts the CPU into long mode (compatibility submode until CS is reloaded).
;
; long_mode_trampoline (64-bit): reached via the far jump in boot32.asm. It runs
; at a LOW (identity-mapped) address, reloads data segments, recovers the
; Multiboot2 magic/info, and calls kmain() in the higher half.

IA32_EFER equ 0xC0000080

extern p4_table         ; paging_boot.asm
extern mb_magic         ; boot32.asm
extern mb_info          ; boot32.asm
extern kmain            ; kernel/core/kmain.c (higher half)

global enable_long_mode_paging
global long_mode_trampoline

; ---------------------------------------------------------------------------
section .boot.text
bits 32
enable_long_mode_paging:
    ; Load CR3 with the physical address of the PML4 (identity: VMA == phys).
    mov eax, p4_table
    mov cr3, eax

    ; Enable PAE (CR4 bit 5) - required for long mode.
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Set Long Mode Enable (and No-Execute Enable) in EFER.
    mov ecx, IA32_EFER
    rdmsr
    or eax, (1 << 8) | (1 << 11)   ; LME | NXE
    wrmsr

    ; Enable paging (CR0 bit 31) -> CPU enters long mode.
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    ret

; ---------------------------------------------------------------------------
section .boot.text
bits 64
long_mode_trampoline:
    ; Reload data segment registers with the kernel data selector (0x10).
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; System V AMD64 ABI: first arg in RDI, second in RSI.
    ; (Zero-extends the 32-bit Multiboot2 magic/info into the 64-bit regs.)
    mov edi, [mb_magic]
    mov esi, [mb_info]

    ; Jump to the higher-half kernel entry. kmain lives at a 64-bit virtual
    ; address, so load it into a register and call indirectly.
    mov rax, kmain
    call rax

.hang:
    cli
    hlt
    jmp .hang
