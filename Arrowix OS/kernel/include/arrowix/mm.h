/*
 * Arrowix OS - Common memory constants and physical/virtual helpers.
 *
 * The kernel keeps a "direct map" of all physical RAM at DIRECT_MAP_BASE, so a
 * physical address can always be reached as DIRECT_MAP_BASE + phys once the VMM
 * has set it up. virt_to_phys is only valid for pointers inside that window.
 */
#pragma once

#include <arrowix/types.h>

#define PAGE_SIZE ARROWIX_PAGE_SIZE /* 4096 */
#define HUGE_PAGE_2M 0x200000ULL

/* PML4[256] -> linear map of physical RAM. */
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL

/* PML4[511]/PDPT[511] (-1 GiB): on-demand kernel heap window. */
#define KHEAP_BASE 0xFFFFFFFFC0000000ULL

#define PAGE_ALIGN_DOWN(x) ((u64) (x) & ~(PAGE_SIZE - 1ULL))
#define PAGE_ALIGN_UP(x)   (((u64) (x) + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL))

#ifdef __cplusplus
extern "C" {
#endif

/* Set to true by vmm_init once the direct map is live. */
extern bool g_direct_map_ready;

static inline void *phys_to_virt(paddr_t phys)
{
    return (void *) (uintptr_t) (phys + DIRECT_MAP_BASE);
}

static inline paddr_t virt_to_phys(void *virt)
{
    return (paddr_t) ((uintptr_t) virt - DIRECT_MAP_BASE);
}

#ifdef __cplusplus
}
#endif
