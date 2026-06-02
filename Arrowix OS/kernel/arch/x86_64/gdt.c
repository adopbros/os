/*
 * Arrowix OS - Runtime Global Descriptor Table + TSS install.
 *
 * Replaces the minimal boot GDT (boot/gdt64.asm) once we are in C. Layout:
 *   [0] null         [1] kernel code (0x08)  [2] kernel data (0x10)
 *   [3] user code    [4] user data           [5..6] TSS (0x28, 16 bytes)
 */

#include <arrowix/gdt.h>
#include <arrowix/tss.h>

/* Common descriptor bits (flat segments; base/limit ignored in long mode). */
#define DESC_PRESENT  (1ULL << 47)
#define DESC_TYPE     (1ULL << 44) /* code/data (not system) */
#define DESC_EXEC     (1ULL << 43)
#define DESC_RW       (1ULL << 41)
#define DESC_LONG     (1ULL << 53)
#define DESC_DPL3     (3ULL << 45)

#define KCODE (DESC_PRESENT | DESC_TYPE | DESC_EXEC | DESC_LONG)
#define KDATA (DESC_PRESENT | DESC_TYPE | DESC_RW)
#define UCODE (KCODE | DESC_DPL3)
#define UDATA (KDATA | DESC_DPL3)

struct __attribute__((packed)) gdt_ptr {
    u16 limit;
    u64 base;
};

/* 5 standard 8-byte descriptors + a 16-byte TSS descriptor (slots 5 and 6). */
static u64 g_gdt[7];

static void set_tss_descriptor(int idx, u64 base, u32 limit)
{
    u64 low = 0;
    low |= (u64) (limit & 0xFFFF);              /* limit 0:15  */
    low |= (base & 0xFFFFFFULL) << 16;          /* base 0:23   */
    low |= 0x9ULL << 40;                        /* type: available 64-bit TSS */
    low |= DESC_PRESENT;                        /* present     */
    low |= (u64) ((limit >> 16) & 0xF) << 48;   /* limit 16:19 */
    low |= ((base >> 24) & 0xFFULL) << 56;      /* base 24:31  */
    g_gdt[idx] = low;
    g_gdt[idx + 1] = (base >> 32) & 0xFFFFFFFFULL; /* base 32:63 */
}

static void gdt_load(struct gdt_ptr *p)
{
    __asm__ volatile("lgdt %0" : : "m"(*p) : "memory");

    /* Reload data segment registers with the kernel data selector. */
    __asm__ volatile("movw %0, %%ax\n"
                     "movw %%ax, %%ds\n"
                     "movw %%ax, %%es\n"
                     "movw %%ax, %%ss\n"
                     "movw %%ax, %%fs\n"
                     "movw %%ax, %%gs\n"
                     :
                     : "i"(GDT_KERNEL_DATA)
                     : "rax");

    /* Reload CS via a far return (cannot mov into CS directly). */
    __asm__ volatile("pushq %0\n"
                     "leaq 1f(%%rip), %%rax\n"
                     "pushq %%rax\n"
                     "lretq\n"
                     "1:\n"
                     :
                     : "i"((u64) GDT_KERNEL_CODE)
                     : "rax", "memory");
}

static void tss_flush(u16 selector)
{
    __asm__ volatile("ltr %0" : : "r"(selector));
}

void gdt_init(void)
{
    g_gdt[0] = 0;
    g_gdt[1] = KCODE;
    g_gdt[2] = KDATA;
    g_gdt[3] = UCODE;
    g_gdt[4] = UDATA;

    tss_init();
    set_tss_descriptor(5, (u64) (uintptr_t) tss_get(), tss_limit());

    struct gdt_ptr ptr = {
        .limit = (u16) (sizeof(g_gdt) - 1),
        .base = (u64) (uintptr_t) &g_gdt,
    };
    gdt_load(&ptr);
    tss_flush(GDT_TSS);
}
