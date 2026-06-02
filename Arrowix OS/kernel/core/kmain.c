/*
 * Arrowix OS - Kernel entry point.
 *
 * Reached from the boot stub (boot/long_mode.asm) in 64-bit long mode with the
 * higher-half mapping active. Phase 2 brings up the architectural core:
 * console, GDT/TSS, IDT, the PIC, and the PIT heartbeat, then enables
 * interrupts and reports the timer ticks.
 *
 * Arguments (System V AMD64 ABI, set up by the trampoline):
 *   mb_magic - the Multiboot2 magic (0x36d76289)
 *   mb_info  - physical address of the Multiboot2 information structure
 */

#include <arrowix/types.h>
#include <arrowix/console.h>
#include <arrowix/gdt.h>
#include <arrowix/idt.h>
#include <arrowix/irq.h>
#include <arrowix/pic.h>
#include <arrowix/pit.h>
#include <arrowix/keyboard.h>
#include <arrowix/mm.h>
#include <arrowix/multiboot2.h>
#include <arrowix/pmm.h>
#include <arrowix/vmm.h>
#include <arrowix/kheap.h>
#include <arrowix/shell.h>
#include <pci.h>

static inline void enable_interrupts(void)
{
    __asm__ volatile("sti");
}

/* Bring up Multiboot2 parsing, the PMM, the direct map, and the kernel heap. */
static void memory_init(u32 mb_info)
{
    mb2_init(mb_info);
    kprintf("[ok] Multiboot2 parsed: %u MiB usable RAM (highest 0x%x)\n",
            (u32) (mb2_total_available() >> 20), (u32) mb2_highest_addr());

    pmm_init();
    struct pmm_stats ps;
    pmm_get_stats(&ps);
    kprintf("[ok] PMM online: %u frames total, %u free (%u MiB free)\n",
            (u32) ps.total_frames, (u32) ps.free_frames,
            (u32) ((ps.free_frames * PAGE_SIZE) >> 20));

    vmm_init();
    kprintf("[ok] VMM online: direct map @ %p\n", (void *) DIRECT_MAP_BASE);

    kheap_init();
    kprintf("[ok] kheap online @ %p\n", (void *) KHEAP_BASE);
}

/* Quick smoke test: allocate, write, free, and reuse heap memory + a VMM map. */
static void memory_selftest(void)
{
    char *a = (char *) kmalloc(64);
    char *b = (char *) kmalloc(4096);
    if (a == NULL || b == NULL) {
        kprintf("[!!] kmalloc returned NULL\n");
        return;
    }
    for (int i = 0; i < 64; ++i) {
        a[i] = (char) i;
    }
    b[0] = 'X';
    b[4095] = 'Z';
    kprintf("[ok] kmalloc a=%p b=%p (a[10]=%d b[4095]=%c)\n",
            (void *) a, (void *) b, (int) a[10], b[4095]);

    kfree(a);
    char *c = (char *) kmalloc(64);
    kprintf("[ok] kfree+reuse: c=%p (%s)\n", (void *) c,
            c == a ? "reused freed block" : "fresh block");
    kfree(b);
    kfree(c);

    /* Map a fresh physical frame at an unused virtual address and round-trip it. */
    paddr_t frame = pmm_alloc_frame();
    /* Far above the mapped heap, still inside the unused kheap PDPT (PML4[511]/
     * PDPT[511]) so the walk creates fresh tables instead of hitting boot's
     * higher-half huge pages. */
    vaddr_t probe = KHEAP_BASE + 0x4000000;
    if (frame != PMM_NO_FRAME &&
        vmm_map(vmm_kernel_space(), probe, frame, PG_PRESENT | PG_WRITE | PG_NX)) {
        volatile u64 *p = (volatile u64 *) probe;
        *p = 0xCAFEBABEULL;
        kprintf("[ok] vmm_map test: virt %p -> phys 0x%x, read 0x%x\n",
                (void *) probe, (u32) vmm_translate(vmm_kernel_space(), probe),
                (u32) *p);
        vmm_unmap(vmm_kernel_space(), probe);
        pmm_free_frame(frame);
    }
}

void kmain(u32 mb_magic, u32 mb_info)
{
    console_init();
    kprintf("Arrowix OS - long mode reached. Booting core...\n");

    if (mb_magic != ARROWIX_MULTIBOOT2_MAGIC) {
        kprintf("WARNING: invalid Multiboot2 magic (0x%x)\n", mb_magic);
    }

    gdt_init();
    kprintf("[ok] GDT + TSS loaded\n");

    idt_init();
    kprintf("[ok] IDT loaded (256 vectors)\n");

    irq_set_controller(pic_controller());
    irq_init();
    kprintf("[ok] PIC remapped (IRQ 0-15 -> vectors 0x20-0x2F)\n");

    pit_init(100);
    /* Drive the persistent taskbar clock from the timer tick for accuracy. */
    pit_set_tick_callback(update_taskbar);
    kprintf("[ok] PIT armed at %u Hz\n", pit_frequency());

    keyboard_init();
    kprintf("[ok] PS/2 keyboard ready (IRQ1)\n");

    memory_init(mb_info);
    memory_selftest();

    /*
     * Real hardware discovery: enumerate the PCI bus and bind drivers (the
     * RTL8139 NIC autoconfigures here). Runs after the VMM so the network
     * driver can reach its DMA buffer through the direct map, and after the PIC
     * so registering the card's IRQ unmasks the correct line.
     */
    pci_probe();

    enable_interrupts();
    kprintf("[ok] interrupts enabled. Launching Arrowix Shell...\n");

    /*
     * Hand control to the interactive presentation layer: boot splash, framed
     * UI (status bar + live clock, central console, shortcut bar), and the
     * keyboard-driven command interpreter. Requires interrupts (PIT/keyboard).
     */
    shell_run();

    /* shell_run never returns; idle defensively just in case. */
    for (;;) {
        __asm__ volatile("hlt");
    }
}
