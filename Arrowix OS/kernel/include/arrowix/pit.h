/*
 * Arrowix OS - Programmable Interval Timer (8253/8254).
 */
#pragma once

#include <arrowix/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional callback invoked on every timer tick (IRQ0 context: keep it short). */
typedef void (*pit_tick_cb_t)(void);
void pit_set_tick_callback(pit_tick_cb_t cb);

/* Program channel 0 at `hz` ticks per second and register the IRQ0 handler. */
void pit_init(u32 hz);

/* Monotonic tick count since pit_init. */
u64 pit_ticks(void);

/* Configured tick frequency (Hz). */
u32 pit_frequency(void);

/* Busy-wait (hlt) for approximately `ms` milliseconds. Requires interrupts. */
void pit_sleep_ms(u32 ms);

#ifdef __cplusplus
}
#endif
