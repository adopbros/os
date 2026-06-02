/*
 * Arrowix OS - Legacy 8259A Programmable Interrupt Controller.
 */
#pragma once

#include <arrowix/types.h>
#include <arrowix/irq.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remap the master/slave PICs to vectors 0x20-0x2F and mask all IRQs. */
void pic_remap(void);

/* Send end-of-interrupt for a logical IRQ (0-15). */
void pic_eoi(u8 irq);

void pic_mask(u8 irq);
void pic_unmask(u8 irq);

/* intr_controller backend backed by the 8259 PIC. */
const struct intr_controller *pic_controller(void);

#ifdef __cplusplus
}
#endif
