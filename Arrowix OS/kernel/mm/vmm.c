/*
 * Arrowix OS - Virtual Memory Manager (4-level paging).
 *
 * Bootstrap problem: to build the direct map we must write page-table frames
 * before the direct map itself exists. We solve it by reaching those frames
 * through the boot identity map (low RAM) while g_direct_map_ready is false;
 * once the direct map is installed we flip the flag and every table is reached
 * through DIRECT_MAP_BASE instead.
 */

#include <arrowix/vmm.h>
#include <arrowix/mm.h>
#include <arrowix/pmm.h>
#include <arrowix/multiboot2.h>
#include <arrowix/string.h>

#define HUGE_ADDR_MASK 0x000FFFFFFFE00000ULL

bool g_direct_map_ready = false;

static addr_space_t g_kernel_pml4;

static inline void *table_ptr(paddr_t phys)
{
    if (g_direct_map_ready) {
        return phys_to_virt(phys);
    }
    /* Pre-direct-map: rely on the boot identity map of low RAM. */
    return (void *) (uintptr_t) phys;
}

static inline paddr_t read_cr3(void)
{
    paddr_t v;
    __asm__ volatile("mov %%cr3, %0" : "=r"(v));
    return v;
}

static inline void flush_tlb_all(void)
{
    paddr_t cr3 = read_cr3();
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static inline void invlpg(vaddr_t v)
{
    __asm__ volatile("invlpg (%0)" : : "r"(v) : "memory");
}

void vmm_init(void)
{
    g_kernel_pml4 = read_cr3() & PG_ADDR_MASK;

    u64 highest = mb2_highest_addr();
    /* Round up so the direct map covers the final partial 2 MiB region. */
    highest = (highest + HUGE_PAGE_2M - 1) & ~(HUGE_PAGE_2M - 1);

    u64 *pml4 = (u64 *) table_ptr(g_kernel_pml4);

    for (u64 phys = 0; phys < highest; phys += HUGE_PAGE_2M) {
        vaddr_t virt = DIRECT_MAP_BASE + phys;
        u64 i4 = (virt >> 39) & 0x1FF;
        u64 i3 = (virt >> 30) & 0x1FF;
        u64 i2 = (virt >> 21) & 0x1FF;

        if (!(pml4[i4] & PG_PRESENT)) {
            paddr_t f = pmm_alloc_frame();
            memset(table_ptr(f), 0, PAGE_SIZE);
            pml4[i4] = f | PG_PRESENT | PG_WRITE;
        }
        u64 *pdpt = (u64 *) table_ptr(pml4[i4] & PG_ADDR_MASK);

        if (!(pdpt[i3] & PG_PRESENT)) {
            paddr_t f = pmm_alloc_frame();
            memset(table_ptr(f), 0, PAGE_SIZE);
            pdpt[i3] = f | PG_PRESENT | PG_WRITE;
        }
        u64 *pd = (u64 *) table_ptr(pdpt[i3] & PG_ADDR_MASK);

        /* 2 MiB huge page: identity-into-direct-map, global, writable, NX. */
        pd[i2] = (phys & HUGE_ADDR_MASK) | PG_PRESENT | PG_WRITE | PG_HUGE | PG_GLOBAL | PG_NX;
    }

    flush_tlb_all();
    g_direct_map_ready = true;
}

addr_space_t vmm_kernel_space(void)
{
    return g_kernel_pml4;
}

static u64 *get_or_create(u64 *table, u64 index, u64 inherit_flags)
{
    if (!(table[index] & PG_PRESENT)) {
        paddr_t f = pmm_alloc_frame();
        if (f == PMM_NO_FRAME) {
            return NULL;
        }
        memset(table_ptr(f), 0, PAGE_SIZE);
        table[index] = f | PG_PRESENT | PG_WRITE | inherit_flags;
    }
    return (u64 *) table_ptr(table[index] & PG_ADDR_MASK);
}

bool vmm_map(addr_space_t space, vaddr_t virt, paddr_t phys, u64 flags)
{
    u64 i4 = (virt >> 39) & 0x1FF;
    u64 i3 = (virt >> 30) & 0x1FF;
    u64 i2 = (virt >> 21) & 0x1FF;
    u64 i1 = (virt >> 12) & 0x1FF;
    u64 inherit = flags & PG_USER;

    u64 *pml4 = (u64 *) table_ptr(space);
    u64 *pdpt = get_or_create(pml4, i4, inherit);
    if (pdpt == NULL) {
        return false;
    }
    u64 *pd = get_or_create(pdpt, i3, inherit);
    if (pd == NULL) {
        return false;
    }
    u64 *pt = get_or_create(pd, i2, inherit);
    if (pt == NULL) {
        return false;
    }

    pt[i1] = (phys & PG_ADDR_MASK) | (flags & ~PG_ADDR_MASK) | PG_PRESENT;
    invlpg(virt);
    return true;
}

/* Walk down to the PT entry for `virt`; returns NULL if any level is missing. */
static u64 *walk_to_pte(addr_space_t space, vaddr_t virt)
{
    u64 i4 = (virt >> 39) & 0x1FF;
    u64 i3 = (virt >> 30) & 0x1FF;
    u64 i2 = (virt >> 21) & 0x1FF;
    u64 i1 = (virt >> 12) & 0x1FF;

    u64 *pml4 = (u64 *) table_ptr(space);
    if (!(pml4[i4] & PG_PRESENT)) {
        return NULL;
    }
    u64 *pdpt = (u64 *) table_ptr(pml4[i4] & PG_ADDR_MASK);
    if (!(pdpt[i3] & PG_PRESENT)) {
        return NULL;
    }
    u64 *pd = (u64 *) table_ptr(pdpt[i3] & PG_ADDR_MASK);
    if (!(pd[i2] & PG_PRESENT) || (pd[i2] & PG_HUGE)) {
        return NULL;
    }
    u64 *pt = (u64 *) table_ptr(pd[i2] & PG_ADDR_MASK);
    return &pt[i1];
}

bool vmm_unmap(addr_space_t space, vaddr_t virt)
{
    u64 *pte = walk_to_pte(space, virt);
    if (pte == NULL || !(*pte & PG_PRESENT)) {
        return false;
    }
    *pte = 0;
    invlpg(virt);
    return true;
}

bool vmm_protect(addr_space_t space, vaddr_t virt, u64 flags)
{
    u64 *pte = walk_to_pte(space, virt);
    if (pte == NULL || !(*pte & PG_PRESENT)) {
        return false;
    }
    *pte = (*pte & PG_ADDR_MASK) | (flags & ~PG_ADDR_MASK) | PG_PRESENT;
    invlpg(virt);
    return true;
}

paddr_t vmm_translate(addr_space_t space, vaddr_t virt)
{
    u64 i4 = (virt >> 39) & 0x1FF;
    u64 i3 = (virt >> 30) & 0x1FF;
    u64 i2 = (virt >> 21) & 0x1FF;
    u64 i1 = (virt >> 12) & 0x1FF;

    u64 *pml4 = (u64 *) table_ptr(space);
    if (!(pml4[i4] & PG_PRESENT)) {
        return VMM_TRANSLATE_FAIL;
    }
    u64 *pdpt = (u64 *) table_ptr(pml4[i4] & PG_ADDR_MASK);
    if (!(pdpt[i3] & PG_PRESENT)) {
        return VMM_TRANSLATE_FAIL;
    }
    u64 *pd = (u64 *) table_ptr(pdpt[i3] & PG_ADDR_MASK);
    if (!(pd[i2] & PG_PRESENT)) {
        return VMM_TRANSLATE_FAIL;
    }
    if (pd[i2] & PG_HUGE) {
        return (pd[i2] & HUGE_ADDR_MASK) | (virt & (HUGE_PAGE_2M - 1));
    }
    u64 *pt = (u64 *) table_ptr(pd[i2] & PG_ADDR_MASK);
    if (!(pt[i1] & PG_PRESENT)) {
        return VMM_TRANSLATE_FAIL;
    }
    return (pt[i1] & PG_ADDR_MASK) | (virt & (PAGE_SIZE - 1));
}
