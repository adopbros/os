/*
 * Arrowix OS - Task State Segment (x86_64).
 *
 * In long mode the TSS no longer holds task context, but it provides:
 *   - RSP0: the stack used on a privilege change (Ring 3 -> Ring 0), Phase 7.
 *   - IST1..7: dedicated interrupt stacks for critical exceptions.
 */
#pragma once

#include <arrowix/types.h>

struct __attribute__((packed)) tss {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iopb_offset;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Zero the TSS and set up RSP0 + IST1 stacks. */
void tss_init(void);

/* Set the Ring 0 stack used on privilege changes (used from Phase 7). */
void tss_set_rsp0(u64 rsp0);

/* Pointer + limit used by gdt.c to build the TSS descriptor. */
struct tss *tss_get(void);
u32 tss_limit(void);

#ifdef __cplusplus
}
#endif
