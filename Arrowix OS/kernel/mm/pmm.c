/*
 * Arrowix OS - Physical Memory Manager (bitmap frame allocator).
 *
 * The bitmap is placed in low RAM right after the kernel image and is reached
 * through the boot identity map (so it works before the direct map exists).
 * Bit value 1 = used, 0 = free.
 */

#include <arrowix/pmm.h>
#include <arrowix/mm.h>
#include <arrowix/multiboot2.h>
#include <arrowix/string.h>
#include <arrowix/console.h>

/* Linker symbols (see linker/kernel.ld). */
extern char _boot_phys_start[];
extern char _kernel_end[];

static u8 *g_bitmap;        /* identity pointer into low RAM */
static u64 g_bitmap_size;   /* bytes */
static u64 g_total_frames;
static u64 g_used_frames;
static u64 g_alloc_hint;    /* frame index to start scanning from */

static inline void bit_set(u64 idx)   { g_bitmap[idx >> 3] |= (u8) (1u << (idx & 7)); }
static inline void bit_clear(u64 idx) { g_bitmap[idx >> 3] &= (u8) ~(1u << (idx & 7)); }
static inline bool bit_test(u64 idx)  { return (g_bitmap[idx >> 3] >> (idx & 7)) & 1u; }

static void frame_mark_used(u64 idx)
{
    if (idx < g_total_frames && !bit_test(idx)) {
        bit_set(idx);
        ++g_used_frames;
    }
}

static void frame_mark_free(u64 idx)
{
    if (idx < g_total_frames && bit_test(idx)) {
        bit_clear(idx);
        --g_used_frames;
    }
}

static void reserve_region(u64 start, u64 end)
{
    u64 first = PAGE_ALIGN_DOWN(start) >> 12;
    u64 last = PAGE_ALIGN_UP(end) >> 12;
    for (u64 i = first; i < last; ++i) {
        frame_mark_used(i);
    }
}

static void free_region(u64 start, u64 end)
{
    u64 first = PAGE_ALIGN_UP(start) >> 12;
    u64 last = PAGE_ALIGN_DOWN(end) >> 12;
    for (u64 i = first; i < last; ++i) {
        frame_mark_free(i);
    }
}

void pmm_init(void)
{
    u64 highest = mb2_highest_addr();
    g_total_frames = highest >> 12;

    u64 kernel_phys_end = (u64) _kernel_end - ARROWIX_KERNEL_VMA;

    /*
     * Place the bitmap in low identity-mapped RAM, past BOTH the kernel image
     * and the live Multiboot2 info (GRUB may park the info just above us, and
     * we must not clobber the mmap we are still reading).
     */
    u64 mb_end = mb2_info_phys() + mb2_info_size();
    u64 place_after = kernel_phys_end > mb_end ? kernel_phys_end : mb_end;
    paddr_t bitmap_phys = PAGE_ALIGN_UP(place_after);
    g_bitmap = (u8 *) (uintptr_t) bitmap_phys;
    g_bitmap_size = (g_total_frames + 7) / 8;

    /* Everything used until we explicitly free the available regions. */
    memset(g_bitmap, 0xFF, g_bitmap_size);
    g_used_frames = g_total_frames;

    /* Free every AVAILABLE region reported by GRUB. */
    u32 count = mb2_mmap_count();
    for (u32 i = 0; i < count; ++i) {
        const struct mb2_mmap_entry *e = mb2_mmap_get(i);
        if (e->type == MB2_MEMORY_AVAILABLE) {
            free_region(e->base_addr, e->base_addr + e->length);
        }
    }

    /* Re-reserve regions that must never be handed out. */
    reserve_region(0, 0x100000);                          /* low 1 MiB (BIOS/IVT) */
    reserve_region((u64) _boot_phys_start, kernel_phys_end); /* boot stub + kernel */
    reserve_region(bitmap_phys, bitmap_phys + g_bitmap_size); /* the bitmap itself */
    reserve_region(mb2_info_phys(), mb2_info_phys() + mb2_info_size()); /* mb2 info */

    g_alloc_hint = 0x100000 >> 12;
}

paddr_t pmm_alloc_frame(void)
{
    for (u64 i = g_alloc_hint; i < g_total_frames; ++i) {
        if (!bit_test(i)) {
            bit_set(i);
            ++g_used_frames;
            g_alloc_hint = i + 1;
            return (paddr_t) i << 12;
        }
    }
    /* Wrap around and retry from the start. */
    for (u64 i = 0; i < g_alloc_hint && i < g_total_frames; ++i) {
        if (!bit_test(i)) {
            bit_set(i);
            ++g_used_frames;
            g_alloc_hint = i + 1;
            return (paddr_t) i << 12;
        }
    }
    return PMM_NO_FRAME;
}

void pmm_free_frame(paddr_t frame)
{
    u64 idx = frame >> 12;
    frame_mark_free(idx);
    if (idx < g_alloc_hint) {
        g_alloc_hint = idx;
    }
}

paddr_t pmm_alloc_frames(size_t n)
{
    if (n == 0) {
        return PMM_NO_FRAME;
    }
    if (n == 1) {
        return pmm_alloc_frame();
    }

    u64 run = 0;
    u64 start = 0;
    for (u64 i = 0x100000 >> 12; i < g_total_frames; ++i) {
        if (!bit_test(i)) {
            if (run == 0) {
                start = i;
            }
            if (++run == n) {
                for (u64 j = start; j < start + n; ++j) {
                    bit_set(j);
                    ++g_used_frames;
                }
                return (paddr_t) start << 12;
            }
        } else {
            run = 0;
        }
    }
    return PMM_NO_FRAME;
}

void pmm_free_frames(paddr_t frame, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        pmm_free_frame(frame + (paddr_t) i * PAGE_SIZE);
    }
}

void pmm_get_stats(struct pmm_stats *out)
{
    if (out == NULL) {
        return;
    }
    out->total_frames = g_total_frames;
    out->used_frames = g_used_frames;
    out->free_frames = g_total_frames - g_used_frames;
}
