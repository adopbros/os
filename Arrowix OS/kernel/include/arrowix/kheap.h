/*
 * Arrowix OS - Kernel heap (free-list allocator).
 *
 * Runs over the on-demand KHEAP_BASE window; grows by mapping more pages
 * (PMM + VMM) when it runs out.
 */
#pragma once

#include <arrowix/types.h>

struct kheap_stats {
    u64 mapped_bytes; /* virtual bytes currently backed by physical frames */
    u64 used_bytes;   /* payload + headers currently allocated */
    u64 free_bytes;   /* free payload available in the pool */
};

#ifdef __cplusplus
extern "C" {
#endif

void kheap_init(void);

void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t alignment);
void kfree(void *ptr);

void kheap_get_stats(struct kheap_stats *out);

#ifdef __cplusplus
}
#endif
