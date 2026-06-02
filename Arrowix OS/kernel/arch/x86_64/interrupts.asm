; Arrowix OS - Interrupt entry stubs (x86_64).
;
; Generates one entry stub per vector (0-255). Each stub normalizes the stack
; into a `struct registers` (see kernel/include/arrowix/isr.h) and calls the
; C++ dispatcher isr_dispatch(). Vectors whose exception pushes a hardware error
; code skip the dummy push so the frame layout is identical for every vector.
;
; isr_stub_table[] (exported) holds the address of each stub; idt.c installs it.

bits 64

extern isr_dispatch

; --- Per-vector stub macros -------------------------------------------------
; No hardware error code: push a dummy 0 so every frame has err_code.
%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push 0          ; dummy error code
    push %1         ; vector number
    jmp isr_common
%endmacro

; Hardware error code already pushed by the CPU: only push the vector.
%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push %1         ; vector number
    jmp isr_common
%endmacro

section .text

; Generate stubs 0..255. Vectors 8,10,11,12,13,14,17,21 push an error code.
%assign v 0
%rep 256
    %if v == 8 || v == 10 || v == 11 || v == 12 || v == 13 || v == 14 || v == 17 || v == 21
        ISR_ERR v
    %else
        ISR_NOERR v
    %endif
    %assign v v + 1
%endrep

; --- Common path: build struct registers, dispatch, restore, iretq ----------
isr_common:
    ; Save general-purpose registers (order must match struct registers).
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp        ; first arg: pointer to the register frame
    cld
    call isr_dispatch

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16         ; discard int_no + err_code
    iretq

; --- Stub address table consumed by idt.c -----------------------------------
section .rodata
global isr_stub_table
isr_stub_table:
%assign v 0
%rep 256
    dq isr_stub_ %+ v
%assign v v + 1
%endrep
