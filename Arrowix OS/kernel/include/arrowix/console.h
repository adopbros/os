/*
 * Arrowix OS - Kernel console (serial COM1 + VGA text) and formatted output.
 *
 * Phase "presentation": besides the plain scrolling console, this exposes VGA
 * primitives (colored cells, absolute positioning, a constrained scroll region)
 * so the Arrowix Shell can paint a status bar, a shortcut bar, and an
 * independently scrolling central area.
 */
#pragma once

#include <arrowix/types.h>
#include <stdarg.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* Standard VGA text-mode palette (foreground 0-15; background 0-7). */
enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LIGHT_GRAY,
    VGA_DARK_GRAY,
    VGA_LIGHT_BLUE,
    VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,
    VGA_LIGHT_MAGENTA,
    VGA_YELLOW,
    VGA_WHITE,
};

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize COM1 and clear the VGA text screen. */
void console_init(void);

/* Low-level output (writes to both serial and VGA). */
void kputc(char c);
void kputs(const char *s);

/*
 * Minimal printf for the kernel. Supported conversions:
 *   %c %s %d %u %x %p %%   with optional 'l'/'ll' length modifiers.
 */
void kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void kvprintf(const char *fmt, va_list ap);

/* --- VGA presentation helpers --------------------------------------------- */

/* Current text color used by kputc/kprintf in the scrolling area. */
void console_set_color(enum vga_color fg, enum vga_color bg);
void console_reset_color(void);

/* Clear the entire screen and home the cursor (ignores the scroll region). */
void console_clear(void);

/* Absolute, color-explicit drawing that does NOT move the text cursor. */
void console_put_at(int x, int y, char c, enum vga_color fg, enum vga_color bg);
void console_write_at(int x, int y, const char *s, enum vga_color fg, enum vga_color bg);
void console_fill_row(int y, char c, enum vga_color fg, enum vga_color bg);

/* Constrain the scrolling text area to rows [top, bottom] (inclusive). */
void console_set_region(int top, int bottom);

/* Clear only the current scroll region and home the cursor inside it. */
void console_clear_region(void);

/*
 * Raw VGA cell save/restore (each cell is attribute<<8 | char). Used to draw a
 * transient overlay (e.g. the Start Menu) and restore the content beneath it.
 */
void console_read_cells(int x, int y, int w, int h, u16 *out);
void console_write_cells(int x, int y, int w, int h, const u16 *in);

#ifdef __cplusplus
}
#endif
