/*
 * Arrowix OS - Realtek RTL8139 Fast Ethernet driver (QEMU default NIC).
 */
#pragma once

#include <arrowix/types.h>

#define RTL8139_VENDOR_ID 0x10ECu
#define RTL8139_DEVICE_ID 0x8139u

#ifdef __cplusplus
extern "C" {
#endif

/* Bring the NIC at the given PCI location online (called from pci_probe). */
void rtl8139_init(u8 bus, u8 dev, u8 func);

/* Pointer to the 6-byte MAC address read from the card (valid after init). */
const u8 *rtl8139_mac(void);

/* Number of frames received since init (incremented from the IRQ handler). */
u64 rtl8139_rx_count(void);

#ifdef __cplusplus
}
#endif
