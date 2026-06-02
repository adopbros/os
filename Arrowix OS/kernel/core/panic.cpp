/*
 * Arrowix OS - Kernel panic (C++).
 *
 * Prints a diagnostic message (and, for CPU exceptions, a full register dump
 * including CR2 for page faults) then halts the CPU forever.
 */

#include <arrowix/panic.h>
#include <arrowix/console.h>
#include <arrowix/isr.h>
#include <stdarg.h>

namespace {

[[noreturn]] void halt_forever()
{
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

u64 read_cr2()
{
    u64 cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

} // namespace

extern "C" [[noreturn]] void panic(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kprintf("\n*** KERNEL PANIC: ");
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf(" ***\n");
    halt_forever();
}

extern "C" [[noreturn]] void panic_regs(const char *msg, struct registers *r)
{
    kprintf("\n*** CPU EXCEPTION: %s ***\n", msg);
    kprintf("  int=%lu  err=%lu\n", r->int_no, r->err_code);
    kprintf("  RIP=%lx  CS=%lx  RFLAGS=%lx\n", r->rip, r->cs, r->rflags);
    kprintf("  RSP=%lx  SS=%lx  CR2=%lx\n", r->rsp, r->ss, read_cr2());
    kprintf("  RAX=%lx  RBX=%lx  RCX=%lx  RDX=%lx\n", r->rax, r->rbx, r->rcx, r->rdx);
    kprintf("  RSI=%lx  RDI=%lx  RBP=%lx\n", r->rsi, r->rdi, r->rbp);
    kprintf("  R8 =%lx  R9 =%lx  R10=%lx  R11=%lx\n", r->r8, r->r9, r->r10, r->r11);
    kprintf("  R12=%lx  R13=%lx  R14=%lx  R15=%lx\n", r->r12, r->r13, r->r14, r->r15);
    kprintf("*** System halted ***\n");
    halt_forever();
}
