# Arrowix OS - Kernel core (architecture, memory, scheduler, syscalls).
# Built as an OBJECT library linked into the final kernel image.

# Sources are added incrementally as phases land. core/kmain is the entry point
# reached from the boot stub.
set(KERNEL_SOURCES
    # core
    core/kmain.c
    core/console.c
    core/panic.cpp
    core/string.c
    core/shell.c
    # arch/x86_64 (Phase 2: GDT/TSS, IDT, interrupt stubs, dispatch, PIC, IRQ)
    arch/x86_64/gdt.c
    arch/x86_64/tss.c
    arch/x86_64/idt.c
    arch/x86_64/pic.c
    arch/x86_64/irq.c
    arch/x86_64/isr.cpp
    arch/x86_64/interrupts.asm
    # mm (Phase 3: Multiboot2 map, PMM, VMM, kernel heap)
    mm/multiboot2.c
    mm/pmm.c
    mm/vmm.c
    mm/kheap.c
    # drivers (Phase 5: real hardware discovery - PCI bus + RTL8139 NIC)
    drivers/pci/pci.c
    drivers/net/rtl8139/rtl8139.c
    # sched/scheduler.c     # Phase 4
)

add_library(arrowix_kernel OBJECT ${KERNEL_SOURCES})

target_include_directories(arrowix_kernel PUBLIC ${ARROWIX_KERNEL_INCLUDE_DIRS})
arrowix_apply_kernel_flags(arrowix_kernel)
