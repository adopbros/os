/*
 * Arrowix OS - Physical Memory Manager (bitmap page-frame allocator).
 *
 * Manages physical RAM in 4 KiB frames. One bit per frame: 1 = used, 0 = free.
 */
#pragma once

#include <arrowix/types.h>

#define PMM_NO_FRAME ((paddr_t) 0) /* allocation failure sentinel */

struct pmm_stats {
    u64 total_frames;
    u64 used_frames;
    u64 free_frames;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Build the frame bitmap from the Multiboot2 memory map (call after mb2_init). */
void pmm_init(void);

/* Allocate / free a single 4 KiB frame. alloc returns PMM_NO_FRAME on failure. */
paddr_t pmm_alloc_frame(void);
void pmm_free_frame(paddr_t frame);

/* Allocate n physically contiguous frames (PMM_NO_FRAME if none). */
paddr_t pmm_alloc_frames(size_t n);
void pmm_free_frames(paddr_t frame, size_t n);

void pmm_get_stats(struct pmm_stats *out);

#ifdef __cplusplus
}
#endif
