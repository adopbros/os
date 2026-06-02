/*
 * Arrowix OS - Interrupt service routine frame and registration.
 */
#pragma once

#include <arrowix/types.h>

/*
 * Register frame as built by interrupts.asm. Field order is significant: it
 * mirrors the push sequence in isr_common plus the CPU-pushed iret frame.
 */
struct registers {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, err_code;          /* pushed by the per-vector stub  */
    u64 rip, cs, rflags, rsp, ss;  /* pushed by the CPU on interrupt */
};

typedef void (*isr_handler_t)(struct registers *);

#ifdef __cplusplus
extern "C" {
#endif

/* Called from interrupts.asm for every vector. */
void isr_dispatch(struct registers *r);

/* Register a handler for any vector (0-255). */
void register_interrupt_handler(u8 vector, isr_handler_t handler);

#ifdef __cplusplus
}
#endif
