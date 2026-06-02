/*
 * Arrowix OS - PCI bus enumeration via the legacy 0xCF8/0xCFC mechanism.
 */

#include <pci.h>
#include <rtl8139.h>
#include <arrowix/io.h>
#include <arrowix/console.h>

/* Build the 32-bit CONFIG_ADDRESS value (enable bit + B/D/F + dword index). */
static inline u32 pci_address(u8 bus, u8 dev, u8 func, u8 offset)
{
    return (u32) (((u32) bus << 16) | ((u32) (dev & 0x1F) << 11) |
                  ((u32) (func & 0x07) << 8) | ((u32) offset & 0xFC) | 0x80000000u);
}

u32 pci_config_read32(u8 bus, u8 dev, u8 func, u8 offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, dev, func, offset));
    return inl(PCI_CONFIG_DATA);
}

u16 pci_config_read16(u8 bus, u8 dev, u8 func, u8 offset)
{
    u32 dword = pci_config_read32(bus, dev, func, offset);
    return (u16) ((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

u8 pci_config_read8(u8 bus, u8 dev, u8 func, u8 offset)
{
    u32 dword = pci_config_read32(bus, dev, func, offset);
    return (u8) ((dword >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 value)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, dev, func, offset));
    outl(PCI_CONFIG_DATA, value);
}

void pci_config_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 value)
{
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, dev, func, offset));
    /* Write only the targeted 16-bit field within the dword. */
    outw((u16) (PCI_CONFIG_DATA + (offset & 2)), value);
}

void pci_enable_bus_mastering(u8 bus, u8 dev, u8 func)
{
    u16 cmd = pci_config_read16(bus, dev, func, PCI_COMMAND);
    cmd |= (u16) (PCI_CMD_IO_SPACE | PCI_CMD_BUS_MASTER);
    pci_config_write16(bus, dev, func, PCI_COMMAND, cmd);
}

static void print_hex(u32 value, int digits)
{
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        u32 nib = (value >> shift) & 0xF;
        kputc((char) (nib < 10 ? '0' + nib : 'a' + nib - 10));
    }
}

static const char *class_desc(u8 cls)
{
    switch (cls) {
    case 0x00: return "Dispositivo legacy";
    case 0x01: return "Almacenamiento";
    case 0x02: return "Controlador de red";
    case 0x03: return "Controlador de video";
    case 0x04: return "Multimedia";
    case 0x06: return "Bridge";
    case 0x0C: return "Bus serie (USB/FW)";
    default:   return "Otro";
    }
}

static void pci_print_row(u8 bus, u8 dev, u8 func, u16 vendor, u16 device, u8 cls, u8 sub)
{
    kputs("  ");
    print_hex(bus, 2);
    kputc(':');
    print_hex(dev, 2);
    kputc('.');
    print_hex(func, 1);
    kputs("      ");
    print_hex(vendor, 4);
    kputs("    ");
    print_hex(device, 4);
    kputs("    ");
    print_hex(cls, 2);
    print_hex(sub, 2);
    kputs("   ");
    kputs(class_desc(cls));
    kputc('\n');
}

void pci_probe(void)
{
    kprintf("\n[PCI] Enumerando bus PCI (puertos 0x%x/0x%x)...\n",
            PCI_CONFIG_ADDRESS, PCI_CONFIG_DATA);
    kputs("  BUS:DEV.FN   VENDOR  DEVICE  CLASS  DESCRIPCION\n");
    kputs("  ----------   ------  ------  -----  -----------\n");

    u32 count = 0;
    for (int bus = 0; bus < 256; ++bus) {
        for (u8 dev = 0; dev < 32; ++dev) {
            if (pci_config_read16((u8) bus, dev, 0, PCI_VENDOR_ID) == PCI_NONE) {
                continue;
            }

            u8 header = pci_config_read8((u8) bus, dev, 0, PCI_HEADER_TYPE);
            u8 functions = (header & PCI_HEADER_MULTIFUNCTION) ? 8 : 1;

            for (u8 func = 0; func < functions; ++func) {
                u16 vendor = pci_config_read16((u8) bus, dev, func, PCI_VENDOR_ID);
                if (vendor == PCI_NONE) {
                    continue;
                }
                u16 device = pci_config_read16((u8) bus, dev, func, PCI_DEVICE_ID);
                u8 cls = pci_config_read8((u8) bus, dev, func, PCI_CLASS);
                u8 sub = pci_config_read8((u8) bus, dev, func, PCI_SUBCLASS);

                pci_print_row((u8) bus, dev, func, vendor, device, cls, sub);
                ++count;

                /* Driver binding: Realtek RTL8139 NIC. */
                if (vendor == RTL8139_VENDOR_ID && device == RTL8139_DEVICE_ID) {
                    rtl8139_init((u8) bus, dev, func);
                }
            }
        }
    }

    kprintf("[PCI] %u dispositivo(s) detectado(s).\n", count);
}
