/*
 * Arrowix OS - 8259A PIC driver.
 *
 * Remaps the master/slave PICs away from the CPU exception vectors (0-31) to
 * 0x20-0x2F, masks every line, and exposes an intr_controller backend so the
 * IRQ layer can later swap in an APIC backend without changes elsewhere.
 */

#include <arrowix/pic.h>
#include <arrowix/io.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20

/* ICW1 / ICW4 bits. */
#define ICW1_INIT 0x11 /* init + expect ICW4 */
#define ICW4_8086 0x01 /* 8086/88 mode */

void pic_remap(void)
{
    outb(PIC1_CMD, ICW1_INIT);  io_wait();
    outb(PIC2_CMD, ICW1_INIT);  io_wait();
    outb(PIC1_DATA, 0x20);      io_wait(); /* master vector offset -> 0x20 */
    outb(PIC2_DATA, 0x28);      io_wait(); /* slave vector offset  -> 0x28 */
    outb(PIC1_DATA, 0x04);      io_wait(); /* master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02);      io_wait(); /* slave cascade identity */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    /* Mask all IRQs; drivers unmask the lines they own. */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_eoi(u8 irq)
{
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask(u8 irq)
{
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8 bit = (irq < 8) ? irq : (u8) (irq - 8);
    u8 mask = inb(port);
    outb(port, mask | (u8) (1u << bit));
}

void pic_unmask(u8 irq)
{
    u16 port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    u8 bit = (irq < 8) ? irq : (u8) (irq - 8);
    u8 mask = inb(port);
    outb(port, mask & (u8) ~(1u << bit));
}

static const struct intr_controller g_pic_controller = {
    .init = pic_remap,
    .mask = pic_mask,
    .unmask = pic_unmask,
    .eoi = pic_eoi,
};

const struct intr_controller *pic_controller(void)
{
    return &g_pic_controller;
}
