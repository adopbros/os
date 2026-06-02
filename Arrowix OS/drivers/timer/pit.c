/*
 * Arrowix OS - PIT (8253/8254) timer driver.
 *
 * Programs channel 0 in mode 3 (square wave) to fire IRQ0 at a fixed rate and
 * maintains a monotonic tick counter - the kernel's first heartbeat.
 */

#include <arrowix/pit.h>
#include <arrowix/io.h>
#include <arrowix/irq.h>

#define PIT_BASE_FREQUENCY 1193182u /* input clock in Hz */
#define PIT_CHANNEL0       0x40
#define PIT_COMMAND        0x43

/* Command: channel 0, access lo+hi byte, mode 3 (square wave), binary. */
#define PIT_CMD_MODE3      0x36

static volatile u64 g_ticks;
static u32 g_frequency;
static pit_tick_cb_t g_tick_cb;

void pit_set_tick_callback(pit_tick_cb_t cb)
{
    g_tick_cb = cb;
}

static void pit_irq_handler(struct registers *r)
{
    (void) r;
    ++g_ticks;
    if (g_tick_cb != NULL) {
        g_tick_cb();
    }
}

void pit_init(u32 hz)
{
    if (hz == 0) {
        hz = 100;
    }
    g_frequency = hz;

    u32 divisor = PIT_BASE_FREQUENCY / hz;
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }

    outb(PIT_COMMAND, PIT_CMD_MODE3);
    outb(PIT_CHANNEL0, (u8) (divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8) ((divisor >> 8) & 0xFF));

    irq_register_handler(0, pit_irq_handler);
}

u64 pit_ticks(void)
{
    return g_ticks;
}

u32 pit_frequency(void)
{
    return g_frequency;
}

void pit_sleep_ms(u32 ms)
{
    if (g_frequency == 0) {
        return;
    }
    u64 target = g_ticks + ((u64) ms * g_frequency) / 1000u;
    while (g_ticks < target) {
        __asm__ volatile("hlt");
    }
}
