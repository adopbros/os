/*
 * Arrowix OS - PCI (Peripheral Component Interconnect) bus subsystem.
 *
 * Legacy configuration access through the two I/O ports:
 *   0xCF8 - CONFIG_ADDRESS (bus/device/function/register selector)
 *   0xCFC - CONFIG_DATA    (32-bit data window)
 */
#pragma once

#include <arrowix/types.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* Configuration space register offsets (type 0 header). */
#define PCI_VENDOR_ID      0x00
#define PCI_DEVICE_ID      0x02
#define PCI_COMMAND        0x04
#define PCI_STATUS         0x06
#define PCI_REVISION_ID    0x08
#define PCI_PROG_IF        0x09
#define PCI_SUBCLASS       0x0A
#define PCI_CLASS          0x0B
#define PCI_HEADER_TYPE    0x0E
#define PCI_BAR0           0x10
#define PCI_BAR1           0x14
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN  0x3D

/* COMMAND register bits. */
#define PCI_CMD_IO_SPACE   (1u << 0)
#define PCI_CMD_MEM_SPACE  (1u << 1)
#define PCI_CMD_BUS_MASTER (1u << 2)

#define PCI_HEADER_MULTIFUNCTION 0x80
#define PCI_NONE 0xFFFFu /* vendor id read from an absent device */

#ifdef __cplusplus
extern "C" {
#endif

/* Raw configuration-space accessors. */
u32 pci_config_read32(u8 bus, u8 dev, u8 func, u8 offset);
u16 pci_config_read16(u8 bus, u8 dev, u8 func, u8 offset);
u8 pci_config_read8(u8 bus, u8 dev, u8 func, u8 offset);
void pci_config_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 value);
void pci_config_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 value);

/* Set the COMMAND register bits for I/O space + bus mastering DMA. */
void pci_enable_bus_mastering(u8 bus, u8 dev, u8 func);

/* Enumerate every bus/device/function, print a table, and bind known drivers. */
void pci_probe(void);

#ifdef __cplusplus
}
#endif
