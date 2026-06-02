/*
 * Arrowix OS - Common kernel types.
 *
 * Freestanding: relies only on the compiler-provided <stdint.h>/<stddef.h>/
 * <stdbool.h> headers (available even without a hosted libc).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Fixed-width integer aliases used throughout the kernel. */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

/* Address types. */
typedef uintptr_t vaddr_t;  /* virtual address */
typedef uint64_t  paddr_t;  /* physical address */

/* Kernel virtual base (must match linker/kernel.ld KERNEL_VMA). */
#define ARROWIX_KERNEL_VMA 0xFFFFFFFF80000000ULL

/* Multiboot2 magic the bootloader passes in EAX -> kmain's first argument. */
#define ARROWIX_MULTIBOOT2_MAGIC 0x36d76289u

#define ARROWIX_PAGE_SIZE 4096ULL
