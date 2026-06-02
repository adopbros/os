/*
 * Arrowix OS - Kernel heap.
 *
 * A first-fit free list with block headers laid out contiguously in the
 * KHEAP_BASE window. Allocation splits oversized free blocks; free() coalesces
 * adjacent free neighbours. When no block fits, the heap grows by mapping more
 * physical frames at its tail.
 */

#include <arrowix/kheap.h>
#include <arrowix/mm.h>
#include <arrowix/pmm.h>
#include <arrowix/vmm.h>
#include <arrowix/string.h>

#define KHEAP_MAGIC      0xA770C0DEu
#define KHEAP_INIT_PAGES 16u   /* 64 KiB initial pool */
#define KHEAP_GROW_PAGES 16u
#define KHEAP_MIN_ALIGN  16u

struct block {
    u64 size;            /* payload bytes (excludes header) */
    struct block *next;  /* address-ordered list */
    u32 free;            /* 1 = free, 0 = in use */
    u32 magic;
};

#define HEADER_SIZE (((sizeof(struct block) + 15u) / 16u) * 16u)

static struct block *g_head;
static vaddr_t g_heap_end;  /* first unmapped virtual address of the window */
static u64 g_used_bytes;

static inline u64 align_up(u64 x, u64 a)
{
    return (x + a - 1) & ~(a - 1);
}

static bool map_pages(vaddr_t start, u64 pages)
{
    for (u64 i = 0; i < pages; ++i) {
        paddr_t f = pmm_alloc_frame();
        if (f == PMM_NO_FRAME) {
            return false;
        }
        if (!vmm_map(vmm_kernel_space(), start + i * PAGE_SIZE, f,
                     PG_PRESENT | PG_WRITE | PG_NX)) {
            pmm_free_frame(f);
            return false;
        }
    }
    return true;
}

void kheap_init(void)
{
    if (!map_pages(KHEAP_BASE, KHEAP_INIT_PAGES)) {
        return;
    }
    u64 mapped = (u64) KHEAP_INIT_PAGES * PAGE_SIZE;
    g_heap_end = KHEAP_BASE + mapped;

    g_head = (struct block *) (uintptr_t) KHEAP_BASE;
    g_head->size = mapped - HEADER_SIZE;
    g_head->next = NULL;
    g_head->free = 1;
    g_head->magic = KHEAP_MAGIC;
    g_used_bytes = 0;
}

/* Merge each free block with its contiguous free successor. */
static void coalesce(void)
{
    struct block *cur = g_head;
    while (cur != NULL && cur->next != NULL) {
        struct block *nxt = cur->next;
        u8 *cur_end = (u8 *) cur + HEADER_SIZE + cur->size;
        if (cur->free && nxt->free && cur_end == (u8 *) nxt) {
            cur->size += HEADER_SIZE + nxt->size;
            cur->next = nxt->next;
            continue; /* try merging with the new successor too */
        }
        cur = cur->next;
    }
}

/* Append `bytes` (rounded to pages) to the tail of the heap window. */
static bool grow_heap(u64 bytes)
{
    u64 pages = align_up(bytes, PAGE_SIZE) / PAGE_SIZE;
    if (pages < KHEAP_GROW_PAGES) {
        pages = KHEAP_GROW_PAGES;
    }
    if (!map_pages(g_heap_end, pages)) {
        return false;
    }

    struct block *blk = (struct block *) (uintptr_t) g_heap_end;
    blk->size = pages * PAGE_SIZE - HEADER_SIZE;
    blk->next = NULL;
    blk->free = 1;
    blk->magic = KHEAP_MAGIC;
    g_heap_end += pages * PAGE_SIZE;

    /* Link to the end of the list. */
    struct block *tail = g_head;
    while (tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = blk;

    coalesce();
    return true;
}

static void split_block(struct block *blk, u64 need)
{
    /* Only split if the remainder can hold a header plus a minimal payload. */
    if (blk->size >= need + HEADER_SIZE + KHEAP_MIN_ALIGN) {
        struct block *rest = (struct block *) ((u8 *) blk + HEADER_SIZE + need);
        rest->size = blk->size - need - HEADER_SIZE;
        rest->next = blk->next;
        rest->free = 1;
        rest->magic = KHEAP_MAGIC;
        blk->size = need;
        blk->next = rest;
    }
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    u64 need = align_up(size, KHEAP_MIN_ALIGN);

    for (int attempt = 0; attempt < 2; ++attempt) {
        for (struct block *b = g_head; b != NULL; b = b->next) {
            if (b->free && b->size >= need) {
                split_block(b, need);
                b->free = 0;
                g_used_bytes += HEADER_SIZE + b->size;
                return (void *) ((u8 *) b + HEADER_SIZE);
            }
        }
        /* No fit: grow once and retry. */
        if (!grow_heap(need + HEADER_SIZE)) {
            return NULL;
        }
    }
    return NULL;
}

void *kmalloc_aligned(size_t size, size_t alignment)
{
    if (alignment <= KHEAP_MIN_ALIGN) {
        return kmalloc(size);
    }
    /* Over-allocate, then return an aligned pointer that stashes the original. */
    void *raw = kmalloc(size + alignment + sizeof(void *));
    if (raw == NULL) {
        return NULL;
    }
    uintptr_t aligned = align_up((uintptr_t) raw + sizeof(void *), alignment);
    ((void **) aligned)[-1] = raw;
    return (void *) aligned;
}

void kfree(void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    struct block *blk = (struct block *) ((u8 *) ptr - HEADER_SIZE);
    if (blk->magic != KHEAP_MAGIC) {
        /* Aligned allocation: recover the original payload pointer. */
        void *raw = ((void **) ptr)[-1];
        blk = (struct block *) ((u8 *) raw - HEADER_SIZE);
        if (blk->magic != KHEAP_MAGIC) {
            return; /* not a heap pointer */
        }
    }

    if (!blk->free) {
        blk->free = 1;
        g_used_bytes -= HEADER_SIZE + blk->size;
    }
    coalesce();
}

void kheap_get_stats(struct kheap_stats *out)
{
    if (out == NULL) {
        return;
    }
    out->mapped_bytes = g_heap_end - KHEAP_BASE;
    out->used_bytes = g_used_bytes;

    u64 freeb = 0;
    for (struct block *b = g_head; b != NULL; b = b->next) {
        if (b->free) {
            freeb += b->size;
        }
    }
    out->free_bytes = freeb;
}
