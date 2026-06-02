/*
 * Arrowix OS - Virtual Memory Manager (4-level x86_64 paging).
 *
 * Builds a direct map of all physical RAM at DIRECT_MAP_BASE and exposes
 * map/unmap/protect/translate over an address space (identified by the physical
 * address of its PML4).
 */
#pragma once

#include <arrowix/types.h>

/* Page-table entry flags. */
#define PG_PRESENT (1ULL << 0)
#define PG_WRITE   (1ULL << 1)
#define PG_USER    (1ULL << 2)
#define PG_PWT     (1ULL << 3)
#define PG_PCD     (1ULL << 4)
#define PG_ACCESSED (1ULL << 5)
#define PG_DIRTY   (1ULL << 6)
#define PG_HUGE    (1ULL << 7)
#define PG_GLOBAL  (1ULL << 8)
#define PG_NX      (1ULL << 63)

/* Physical address bits inside a page-table entry (4 KiB aligned, 52-bit PA). */
#define PG_ADDR_MASK 0x000FFFFFFFFFF000ULL

/* An address space is identified by the physical address of its PML4 table. */
typedef paddr_t addr_space_t;

#define VMM_TRANSLATE_FAIL ((paddr_t) ~0ULL)

#ifdef __cplusplus
extern "C" {
#endif

/* Construct the direct map and switch phys_to_virt over to it. */
void vmm_init(void);

/* The kernel address space (current CR3). */
addr_space_t vmm_kernel_space(void);

/* Map / unmap / change protection of a single 4 KiB page. Returns true on success. */
bool vmm_map(addr_space_t space, vaddr_t virt, paddr_t phys, u64 flags);
bool vmm_unmap(addr_space_t space, vaddr_t virt);
bool vmm_protect(addr_space_t space, vaddr_t virt, u64 flags);

/* Resolve a virtual address to its physical address (VMM_TRANSLATE_FAIL if unmapped). */
paddr_t vmm_translate(addr_space_t space, vaddr_t virt);

#ifdef __cplusplus
}
#endif
