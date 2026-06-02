/*
 * Arrowix OS - IDT setup.
 *
 * All 256 vectors point at the per-vector stubs generated in interrupts.asm
 * (exported via isr_stub_table). Critical exceptions use a dedicated IST stack.
 */

#include <arrowix/idt.h>
#include <arrowix/gdt.h>

/* Per-vector entry stubs (addresses), defined in interrupts.asm. */
extern void *isr_stub_table[256];

static struct idt_entry g_idt[256];
static struct idt_ptr   g_idtr;

void idt_set_gate(u8 vector, u64 handler, u8 ist, u8 type_attr)
{
    struct idt_entry *e = &g_idt[vector];
    e->offset_low = (u16) (handler & 0xFFFF);
    e->selector = GDT_KERNEL_CODE;
    e->ist = ist & 0x7;
    e->type_attr = type_attr;
    e->offset_mid = (u16) ((handler >> 16) & 0xFFFF);
    e->offset_high = (u32) ((handler >> 32) & 0xFFFFFFFF);
    e->zero = 0;
}

void idt_init(void)
{
    for (int i = 0; i < 256; ++i) {
        idt_set_gate((u8) i, (u64) (uintptr_t) isr_stub_table[i], 0, IDT_INTERRUPT_GATE);
    }

    /* Route the most dangerous faults onto the dedicated IST1 stack so a bad
     * kernel stack does not turn a fault into a triple fault. */
    idt_set_gate(8, (u64) (uintptr_t) isr_stub_table[8], 1, IDT_INTERRUPT_GATE);   /* #DF */
    idt_set_gate(14, (u64) (uintptr_t) isr_stub_table[14], 1, IDT_INTERRUPT_GATE); /* #PF */

    g_idtr.limit = (u16) (sizeof(g_idt) - 1);
    g_idtr.base = (u64) (uintptr_t) &g_idt;

    __asm__ volatile("lidt %0" : : "m"(g_idtr) : "memory");
}
