/*
 * Arrowix OS - Kernel panic and assertions.
 */
#pragma once

#include <arrowix/types.h>

struct registers; /* forward declaration (see <arrowix/isr.h>) */

#ifdef __cplusplus
extern "C" {
#endif

/* Print a message and halt the CPU. Never returns. */
__attribute__((noreturn)) void panic(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/* Dump a CPU exception (message + register frame) and halt. Never returns. */
__attribute__((noreturn)) void panic_regs(const char *msg, struct registers *r);

#ifdef __cplusplus
}
#endif

#define KASSERT(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            panic("assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__);  \
        }                                                                      \
    } while (0)
