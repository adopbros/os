/*
 * Arrowix OS - Interrupt dispatch (C++).
 *
 * isr_dispatch() is the single entry point called from interrupts.asm. It
 * routes CPU exceptions (0-31) to registered handlers or panic, and hardware
 * IRQs (32-47) to the IRQ layer. Compiled freestanding (no RTTI/exceptions);
 * no global objects with constructors so we need no init_array.
 */

#include <arrowix/isr.h>
#include <arrowix/irq.h>
#include <arrowix/panic.h>

namespace {

constexpr u64 EXCEPTION_COUNT = 32;

const char *const kExceptionNames[EXCEPTION_COUNT] = {
    "Divide-by-Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 Floating-Point (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point (#XM)",
    "Virtualization (#VE)",
    "Control Protection (#CP)",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication",
};

isr_handler_t g_handlers[256];

} // namespace

extern "C" void register_interrupt_handler(u8 vector, isr_handler_t handler)
{
    g_handlers[vector] = handler;
}

extern "C" void isr_dispatch(struct registers *r)
{
    const u64 vec = r->int_no;

    if (vec < EXCEPTION_COUNT) {
        if (g_handlers[vec] != nullptr) {
            g_handlers[vec](r);
            return;
        }
        panic_regs(kExceptionNames[vec], r);
        /* not reached */
    }

    if (vec >= IRQ_BASE_VECTOR && vec < IRQ_BASE_VECTOR + IRQ_COUNT) {
        irq_dispatch(r);
        return;
    }

    /* Software / unhandled vector. */
    if (g_handlers[vec] != nullptr) {
        g_handlers[vec](r);
    }
}
