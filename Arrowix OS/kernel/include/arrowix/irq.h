/*
 * Arrowix OS - Hardware IRQ layer.
 *
 * Abstracts the interrupt controller (PIC now, APIC later) behind a small
 * vtable so device drivers register against logical IRQ numbers (0-15) without
 * caring which controller delivers them.
 */
#pragma once

#include <arrowix/types.h>
#include <arrowix/isr.h>

/* PIC-remapped IRQs occupy vectors 32..47. */
#define IRQ_BASE_VECTOR 32
#define IRQ_COUNT       16

typedef void (*irq_handler_t)(struct registers *);

/* Pluggable interrupt-controller backend (PIC today, APIC tomorrow). */
struct intr_controller {
    void (*init)(void);
    void (*mask)(u8 irq);
    void (*unmask)(u8 irq);
    void (*eoi)(u8 irq);
};

#ifdef __cplusplus
extern "C" {
#endif

/* Select the active controller backend and initialize it. */
void irq_set_controller(const struct intr_controller *ctrl);
void irq_init(void);

/* Register/remove a handler for a logical IRQ (0-15); unmasks on register. */
void irq_register_handler(u8 irq, irq_handler_t handler);

/* Dispatch entry called by isr_dispatch for vectors 32..47. */
void irq_dispatch(struct registers *r);

void irq_mask(u8 irq);
void irq_unmask(u8 irq);

#ifdef __cplusplus
}
#endif
