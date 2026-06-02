/*
 * Arrowix OS - IRQ layer.
 *
 * Sits between isr_dispatch and device drivers. Holds the active interrupt
 * controller backend and a table of per-IRQ handlers, and guarantees the EOI
 * is sent after the handler runs.
 */

#include <arrowix/irq.h>
#include <arrowix/pic.h>

static const struct intr_controller *g_ctrl;
static irq_handler_t g_irq_handlers[IRQ_COUNT];

void irq_set_controller(const struct intr_controller *ctrl)
{
    g_ctrl = ctrl;
}

void irq_init(void)
{
    if (g_ctrl == NULL) {
        g_ctrl = pic_controller();
    }
    g_ctrl->init();
}

void irq_register_handler(u8 irq, irq_handler_t handler)
{
    if (irq >= IRQ_COUNT) {
        return;
    }
    g_irq_handlers[irq] = handler;
    if (g_ctrl != NULL) {
        g_ctrl->unmask(irq);
    }
}

void irq_mask(u8 irq)
{
    if (g_ctrl != NULL && irq < IRQ_COUNT) {
        g_ctrl->mask(irq);
    }
}

void irq_unmask(u8 irq)
{
    if (g_ctrl != NULL && irq < IRQ_COUNT) {
        g_ctrl->unmask(irq);
    }
}

void irq_dispatch(struct registers *r)
{
    u8 irq = (u8) (r->int_no - IRQ_BASE_VECTOR);

    if (irq < IRQ_COUNT && g_irq_handlers[irq] != NULL) {
        g_irq_handlers[irq](r);
    }

    if (g_ctrl != NULL) {
        g_ctrl->eoi(irq);
    }
}
