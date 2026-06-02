/*
 * Arrowix OS - Multiboot2 boot information parser.
 *
 * GRUB hands us (in EBX -> kmain's mb_info) the physical address of a Multiboot2
 * information structure: a u32 total_size, a u32 reserved, then a sequence of
 * 8-byte-aligned tags terminated by an end tag (type 0). We care primarily
 * about the memory map (type 6).
 */
#pragma once

#include <arrowix/types.h>

#define MB2_TAG_TYPE_END           0
#define MB2_TAG_TYPE_BASIC_MEMINFO 4
#define MB2_TAG_TYPE_MMAP          6

/* Memory map entry types. */
#define MB2_MEMORY_AVAILABLE        1
#define MB2_MEMORY_RESERVED         2
#define MB2_MEMORY_ACPI_RECLAIMABLE 3
#define MB2_MEMORY_NVS              4
#define MB2_MEMORY_BADRAM           5

struct __attribute__((packed)) mb2_tag {
    u32 type;
    u32 size;
};

struct __attribute__((packed)) mb2_tag_mmap {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    /* entries follow */
};

struct __attribute__((packed)) mb2_mmap_entry {
    u64 base_addr;
    u64 length;
    u32 type;
    u32 reserved;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Parse the Multiboot2 info located at the given physical address. */
void mb2_init(u64 info_phys);

/* Memory map access. */
u32 mb2_mmap_count(void);
const struct mb2_mmap_entry *mb2_mmap_get(u32 index);

/* Highest physical address that backs RAM (end of the top available region). */
u64 mb2_highest_addr(void);

/* Total bytes reported as available (usable) RAM. */
u64 mb2_total_available(void);

/* Physical address of the Multiboot2 info structure and its total size. */
u64 mb2_info_phys(void);
u32 mb2_info_size(void);

#ifdef __cplusplus
}
#endif
