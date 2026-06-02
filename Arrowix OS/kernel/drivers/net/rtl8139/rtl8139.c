/*
 * Arrowix OS - Realtek RTL8139 driver.
 *
 * Real hardware bring-up: enable PCI bus mastering, power the chip on, software
 * reset it, install a DMA receive ring (physical frames from the PMM), program
 * the receive configuration, read the MAC from the I/O registers, and hook the
 * card's PCI interrupt line into the IRQ layer.
 */

#include <rtl8139.h>
#include <pci.h>
#include <arrowix/io.h>
#include <arrowix/irq.h>
#include <arrowix/isr.h>
#include <arrowix/console.h>
#include <arrowix/pmm.h>
#include <arrowix/mm.h>
#include <arrowix/string.h>

/* Register offsets from the I/O base (BAR0). */
#define RTL_IDR0    0x00 /* MAC bytes 0..5 */
#define RTL_RBSTART 0x30 /* RX buffer physical address */
#define RTL_CMD     0x37 /* command register */
#define RTL_CAPR    0x38 /* current address of packet read */
#define RTL_IMR     0x3C /* interrupt mask */
#define RTL_ISR     0x3E /* interrupt status */
#define RTL_RCR     0x44 /* receive configuration */
#define RTL_CONFIG1 0x52 /* power management */

/* CMD bits. */
#define RTL_CMD_RX_ENABLE 0x08
#define RTL_CMD_TX_ENABLE 0x04
#define RTL_CMD_RESET     0x10
#define RTL_CMD_BUFE      0x01 /* RX buffer empty */

/* Interrupt bits (IMR/ISR). */
#define RTL_INT_ROK 0x0001 /* receive OK */
#define RTL_INT_TOK 0x0004 /* transmit OK */

/* Receive configuration: accept broadcast/multicast/physical-match/all + wrap. */
#define RTL_RCR_AAP  (1u << 0) /* accept all packets (promiscuous) */
#define RTL_RCR_APM  (1u << 1) /* accept physical match */
#define RTL_RCR_AM   (1u << 2) /* accept multicast */
#define RTL_RCR_AB   (1u << 3) /* accept broadcast */
#define RTL_RCR_WRAP (1u << 7) /* wrap (needs pad after the 8 KiB buffer) */

/* 8192 + 16 header + 1500 wrap padding -> 3 frames (12 KiB) is enough. */
#define RTL_RX_FRAMES 3
#define RTL_RX_SIZE   (RTL_RX_FRAMES * PAGE_SIZE)

static u16 g_io;
static u8 g_irq_line;
static u8 g_mac[6];
static paddr_t g_rx_phys;
static u8 *g_rx;
static volatile u64 g_rx_count;

static void print_hex2(u8 v)
{
    const char *d = "0123456789abcdef";
    kputc(d[(v >> 4) & 0xF]);
    kputc(d[v & 0xF]);
}

static void rtl8139_irq(struct registers *r)
{
    (void) r;
    if (g_io == 0) {
        return;
    }

    u16 status = inw(g_io + RTL_ISR);
    /* Acknowledge by writing the bits back. */
    outw(g_io + RTL_ISR, status);

    if (status & RTL_INT_ROK) {
        ++g_rx_count;
    }
}

void rtl8139_init(u8 bus, u8 dev, u8 func)
{
    /* 1. Enable I/O space + bus mastering so the chip can DMA. */
    pci_enable_bus_mastering(bus, dev, func);

    /* 2. Resolve the I/O base from BAR0 (bit 0 set => I/O-mapped). */
    u32 bar0 = pci_config_read32(bus, dev, func, PCI_BAR0);
    if (!(bar0 & 1u)) {
        kprintf("[NET] RTL8139: BAR0 no es espacio de I/O (0x%x)\n", bar0);
        return;
    }
    g_io = (u16) (bar0 & 0xFFFCu);

    /* 3. Power on the device (CONFIG1 = 0 clears LWAKE/sleep). */
    outb(g_io + RTL_CONFIG1, 0x00);

    /* 4. Software reset and wait for the chip to clear the RST bit. */
    outb(g_io + RTL_CMD, RTL_CMD_RESET);
    while (inb(g_io + RTL_CMD) & RTL_CMD_RESET) {
        /* spin until reset completes */
    }

    /* 5. Allocate a physically contiguous DMA receive ring (< 4 GiB). */
    g_rx_phys = pmm_alloc_frames(RTL_RX_FRAMES);
    if (g_rx_phys == PMM_NO_FRAME) {
        kprintf("[NET] RTL8139: sin memoria para el buffer RX\n");
        return;
    }
    g_rx = (u8 *) phys_to_virt(g_rx_phys);
    memset(g_rx, 0, RTL_RX_SIZE);
    outl(g_io + RTL_RBSTART, (u32) g_rx_phys);

    /* 6. Enable the RX OK / TX OK interrupts. */
    outw(g_io + RTL_IMR, RTL_INT_ROK | RTL_INT_TOK);

    /* 7. Receive configuration: accept everything, wrap mode, 8 KiB ring. */
    outl(g_io + RTL_RCR,
         RTL_RCR_AAP | RTL_RCR_APM | RTL_RCR_AM | RTL_RCR_AB | RTL_RCR_WRAP);

    /* 8. Enable the receiver and transmitter. */
    outb(g_io + RTL_CMD, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);

    /* 9. Read the burned-in MAC address from the I/O registers. */
    for (int i = 0; i < 6; ++i) {
        g_mac[i] = inb(g_io + RTL_IDR0 + i);
    }

    /* 10. Hook the PCI-assigned interrupt line into the IRQ layer. */
    g_irq_line = pci_config_read8(bus, dev, func, PCI_INTERRUPT_LINE);
    irq_register_handler(g_irq_line, rtl8139_irq);
    /* IRQs 8..15 arrive through the slave PIC; unmask the cascade line. */
    if (g_irq_line >= 8) {
        irq_unmask(2);
    }

    kprintf("[NET] RTL8139 Inicializado. MAC: ");
    for (int i = 0; i < 6; ++i) {
        print_hex2(g_mac[i]);
        if (i < 5) {
            kputc(':');
        }
    }
    kprintf("  (IO base 0x%x, IRQ %u)\n", g_io, g_irq_line);
}

const u8 *rtl8139_mac(void)
{
    return g_mac;
}

u64 rtl8139_rx_count(void)
{
    return g_rx_count;
}
