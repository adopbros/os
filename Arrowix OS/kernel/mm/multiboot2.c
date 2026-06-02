/*
 * Arrowix OS - Multiboot2 information parser.
 *
 * Walks the tag list once at boot. The info structure lives in low physical
 * memory, reachable through the boot identity map, so we read it directly.
 */

#include <arrowix/multiboot2.h>

static u64 g_info_phys;
static u32 g_info_size;

static const struct mb2_tag_mmap *g_mmap;
static u64 g_highest_addr;
static u64 g_total_available;

static inline const void *phys_ptr(u64 phys)
{
    /* Identity-mapped low memory during early boot. */
    return (const void *) (uintptr_t) phys;
}

void mb2_init(u64 info_phys)
{
    g_info_phys = info_phys;
    g_mmap = NULL;
    g_highest_addr = 0;
    g_total_available = 0;

    const u8 *base = (const u8 *) phys_ptr(info_phys);
    g_info_size = *(const u32 *) base; /* total_size */

    /* Tags start after total_size (u32) + reserved (u32). */
    const u8 *p = base + 8;
    const u8 *end = base + g_info_size;

    while (p < end) {
        const struct mb2_tag *tag = (const struct mb2_tag *) p;
        if (tag->type == MB2_TAG_TYPE_END) {
            break;
        }

        if (tag->type == MB2_TAG_TYPE_MMAP) {
            g_mmap = (const struct mb2_tag_mmap *) tag;
        }

        /* Advance to the next tag (sizes are padded up to 8 bytes). */
        p += (tag->size + 7) & ~((u32) 7);
    }

    /* Compute highest backed address and total available RAM. */
    u32 count = mb2_mmap_count();
    for (u32 i = 0; i < count; ++i) {
        const struct mb2_mmap_entry *e = mb2_mmap_get(i);
        if (e->type != MB2_MEMORY_AVAILABLE) {
            continue;
        }
        u64 region_end = e->base_addr + e->length;
        if (region_end > g_highest_addr) {
            g_highest_addr = region_end;
        }
        g_total_available += e->length;
    }
}

u32 mb2_mmap_count(void)
{
    if (g_mmap == NULL) {
        return 0;
    }
    u32 entries_bytes = g_mmap->size - sizeof(struct mb2_tag_mmap);
    return entries_bytes / g_mmap->entry_size;
}

const struct mb2_mmap_entry *mb2_mmap_get(u32 index)
{
    if (g_mmap == NULL || index >= mb2_mmap_count()) {
        return NULL;
    }
    const u8 *entries = (const u8 *) g_mmap + sizeof(struct mb2_tag_mmap);
    return (const struct mb2_mmap_entry *) (entries + (u64) index * g_mmap->entry_size);
}

u64 mb2_highest_addr(void)
{
    return g_highest_addr;
}

u64 mb2_total_available(void)
{
    return g_total_available;
}

u64 mb2_info_phys(void)
{
    return g_info_phys;
}

u32 mb2_info_size(void)
{
    return g_info_size;
}
