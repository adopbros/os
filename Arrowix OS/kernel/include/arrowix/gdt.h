/*
 * Arrowix OS - Global Descriptor Table (runtime).
 *
 * Segment selectors. The runtime GDT (kernel/arch/x86_64/gdt.c) supersedes the
 * minimal boot GDT (boot/gdt64.asm) and adds user segments + a TSS descriptor.
 */
#pragma once

#include <arrowix/types.h>

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

#ifdef __cplusplus
extern "C" {
#endif

/* Build and load the runtime GDT, then load the TSS (ltr). */
void gdt_init(void);

#ifdef __cplusplus
}
#endif
