/*
 * Arrowix OS - Task State Segment setup.
 */

#include <arrowix/tss.h>

#define IST1_STACK_SIZE   16384 /* 16 KiB stack for critical exceptions */
#define RSP0_STACK_SIZE   16384 /* 16 KiB Ring 0 stack (used from Phase 7) */

static struct tss g_tss;

/* 16-byte aligned dedicated stacks. */
static u8 ist1_stack[IST1_STACK_SIZE] __attribute__((aligned(16)));
static u8 rsp0_stack[RSP0_STACK_SIZE] __attribute__((aligned(16)));

void tss_init(void)
{
    u8 *p = (u8 *) &g_tss;
    for (unsigned i = 0; i < sizeof(g_tss); ++i) {
        p[i] = 0;
    }

    /* Stacks grow downward: point at the top (highest address). */
    g_tss.ist1 = (u64) (ist1_stack + IST1_STACK_SIZE);
    g_tss.rsp0 = (u64) (rsp0_stack + RSP0_STACK_SIZE);

    /* No I/O permission bitmap: set the offset past the TSS limit. */
    g_tss.iopb_offset = sizeof(struct tss);
}

void tss_set_rsp0(u64 rsp0)
{
    g_tss.rsp0 = rsp0;
}

struct tss *tss_get(void)
{
    return &g_tss;
}

u32 tss_limit(void)
{
    return sizeof(struct tss) - 1;
}
