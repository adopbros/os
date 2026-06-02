/*
 * Arrowix OS - Interrupt Descriptor Table (x86_64).
 */
#pragma once

#include <arrowix/types.h>

/* 64-bit interrupt/trap gate descriptor (16 bytes). */
struct __attribute__((packed)) idt_entry {
    u16 offset_low;   /* handler offset bits 0:15   */
    u16 selector;     /* code segment selector      */
    u8  ist;          /* bits 0:2 = IST index, rest 0 */
    u8  type_attr;    /* P | DPL | 0 | gate type    */
    u16 offset_mid;   /* handler offset bits 16:31  */
    u32 offset_high;  /* handler offset bits 32:63  */
    u32 zero;         /* reserved                   */
};

struct __attribute__((packed)) idt_ptr {
    u16 limit;
    u64 base;
};

/* Gate type: present, DPL=0, 64-bit interrupt gate (clears IF on entry). */
#define IDT_INTERRUPT_GATE 0x8E
/* Gate type: present, DPL=0, 64-bit trap gate (leaves IF unchanged). */
#define IDT_TRAP_GATE      0x8F

#ifdef __cplusplus
extern "C" {
#endif

void idt_init(void);
void idt_set_gate(u8 vector, u64 handler, u8 ist, u8 type_attr);

#ifdef __cplusplus
}
#endif
