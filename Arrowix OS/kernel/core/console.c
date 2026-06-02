/*
 * Arrowix OS - Kernel console: COM1 serial + VGA text output, plus a minimal
 * kprintf. Supports a constrained scroll region so the shell can keep a status
 * bar (top) and a shortcut bar (bottom) pinned while the central area scrolls.
 */

#include <arrowix/console.h>
#include <arrowix/io.h>

/* --- VGA text mode (identity-mapped within the boot 1 GiB mapping) --------- */
#define VGA_BUFFER ((volatile u16 *) 0xB8000)
#define VGA_DEFAULT_ATTR 0x0F00u /* white on black */

static int vga_row;
static int vga_col;
static int vga_top = 0;                 /* scroll region (inclusive) */
static int vga_bottom = VGA_HEIGHT - 1;
static u16 vga_attr = VGA_DEFAULT_ATTR; /* current attribute, high byte */

/* --- COM1 serial ----------------------------------------------------------- */
#define COM1 0x3F8

static void serial_init(void)
{
    outb(COM1 + 1, 0x00); /* disable interrupts */
    outb(COM1 + 3, 0x80); /* enable DLAB */
    outb(COM1 + 0, 0x03); /* divisor low: 38400 baud */
    outb(COM1 + 1, 0x00); /* divisor high */
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* enable + clear FIFO */
    outb(COM1 + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static void serial_putc(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0) {
        /* wait for the transmit holding register to be empty */
    }
    outb(COM1, (u8) c);
}

/* --- VGA helpers ----------------------------------------------------------- */
static inline u16 attr_cell(enum vga_color fg, enum vga_color bg, char c)
{
    return (u16) (((u8) bg << 4) | ((u8) fg & 0x0F)) << 8 | (u8) c;
}

static void vga_move_cursor(void)
{
    u16 pos = (u16) (vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 14);
    outb(0x3D5, (u8) (pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (u8) (pos & 0xFF));
}

static void vga_scroll(void)
{
    for (int row = vga_top + 1; row <= vga_bottom; ++row) {
        for (int col = 0; col < VGA_WIDTH; ++col) {
            VGA_BUFFER[(row - 1) * VGA_WIDTH + col] = VGA_BUFFER[row * VGA_WIDTH + col];
        }
    }
    for (int col = 0; col < VGA_WIDTH; ++col) {
        VGA_BUFFER[vga_bottom * VGA_WIDTH + col] = vga_attr | (u16) ' ';
    }
    vga_row = vga_bottom;
}

static void vga_putc(char c)
{
    if (c == '\n') {
        vga_col = 0;
        ++vga_row;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            --vga_col;
        } else if (vga_row > vga_top) {
            --vga_row;
            vga_col = VGA_WIDTH - 1;
        }
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_attr | (u16) ' ';
    } else {
        VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = vga_attr | (u8) c;
        if (++vga_col >= VGA_WIDTH) {
            vga_col = 0;
            ++vga_row;
        }
    }

    if (vga_row > vga_bottom) {
        vga_scroll();
    }
    vga_move_cursor();
}

/* --- Public API ------------------------------------------------------------ */
void console_init(void)
{
    serial_init();
    console_clear();
}

void console_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA_BUFFER[i] = VGA_DEFAULT_ATTR | (u16) ' ';
    }
    vga_top = 0;
    vga_bottom = VGA_HEIGHT - 1;
    vga_row = 0;
    vga_col = 0;
    vga_move_cursor();
}

void console_set_color(enum vga_color fg, enum vga_color bg)
{
    vga_attr = (u16) (((u8) bg << 4) | ((u8) fg & 0x0F)) << 8;
}

void console_reset_color(void)
{
    vga_attr = VGA_DEFAULT_ATTR;
}

void console_put_at(int x, int y, char c, enum vga_color fg, enum vga_color bg)
{
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) {
        return;
    }
    VGA_BUFFER[y * VGA_WIDTH + x] = attr_cell(fg, bg, c);
}

void console_write_at(int x, int y, const char *s, enum vga_color fg, enum vga_color bg)
{
    for (; *s && x < VGA_WIDTH; ++s, ++x) {
        console_put_at(x, y, *s, fg, bg);
    }
}

void console_fill_row(int y, char c, enum vga_color fg, enum vga_color bg)
{
    if (y < 0 || y >= VGA_HEIGHT) {
        return;
    }
    for (int x = 0; x < VGA_WIDTH; ++x) {
        VGA_BUFFER[y * VGA_WIDTH + x] = attr_cell(fg, bg, c);
    }
}

void console_set_region(int top, int bottom)
{
    if (top < 0) {
        top = 0;
    }
    if (bottom > VGA_HEIGHT - 1) {
        bottom = VGA_HEIGHT - 1;
    }
    vga_top = top;
    vga_bottom = bottom;
    vga_row = top;
    vga_col = 0;
    vga_move_cursor();
}

void console_clear_region(void)
{
    for (int y = vga_top; y <= vga_bottom; ++y) {
        for (int x = 0; x < VGA_WIDTH; ++x) {
            VGA_BUFFER[y * VGA_WIDTH + x] = vga_attr | (u16) ' ';
        }
    }
    vga_row = vga_top;
    vga_col = 0;
    vga_move_cursor();
}

void console_read_cells(int x, int y, int w, int h, u16 *out)
{
    int idx = 0;
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int cx = x + i;
            int cy = y + j;
            if (cx >= 0 && cx < VGA_WIDTH && cy >= 0 && cy < VGA_HEIGHT) {
                out[idx] = VGA_BUFFER[cy * VGA_WIDTH + cx];
            } else {
                out[idx] = VGA_DEFAULT_ATTR | (u16) ' ';
            }
            ++idx;
        }
    }
}

void console_write_cells(int x, int y, int w, int h, const u16 *in)
{
    int idx = 0;
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            int cx = x + i;
            int cy = y + j;
            if (cx >= 0 && cx < VGA_WIDTH && cy >= 0 && cy < VGA_HEIGHT) {
                VGA_BUFFER[cy * VGA_WIDTH + cx] = in[idx];
            }
            ++idx;
        }
    }
}

void kputc(char c)
{
    if (c == '\n') {
        serial_putc('\r');
        serial_putc('\n');
    } else if (c == '\b') {
        serial_putc('\b');
        serial_putc(' ');
        serial_putc('\b');
    } else {
        serial_putc(c);
    }
    vga_putc(c);
}

void kputs(const char *s)
{
    for (; *s; ++s) {
        kputc(*s);
    }
}

static void print_uint(unsigned long long val, unsigned base, int upper)
{
    char buf[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (val == 0) {
        buf[i++] = '0';
    }
    while (val != 0) {
        buf[i++] = digits[val % base];
        val /= base;
    }
    while (i-- > 0) {
        kputc(buf[i]);
    }
}

void kvprintf(const char *fmt, va_list ap)
{
    for (; *fmt != '\0'; ++fmt) {
        if (*fmt != '%') {
            kputc(*fmt);
            continue;
        }

        ++fmt;
        int is_long = 0;
        while (*fmt == 'l') {
            ++is_long;
            ++fmt;
        }

        switch (*fmt) {
        case 'c':
            kputc((char) va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            kputs(s ? s : "(null)");
            break;
        }
        case 'd': {
            long long v = is_long ? va_arg(ap, long) : va_arg(ap, int);
            if (v < 0) {
                kputc('-');
                print_uint((unsigned long long) (-v), 10, 0);
            } else {
                print_uint((unsigned long long) v, 10, 0);
            }
            break;
        }
        case 'u': {
            unsigned long long v =
                is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            print_uint(v, 10, 0);
            break;
        }
        case 'x': {
            unsigned long long v =
                is_long ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            print_uint(v, 16, 0);
            break;
        }
        case 'p':
            kputs("0x");
            print_uint((unsigned long long) (uintptr_t) va_arg(ap, void *), 16, 0);
            break;
        case '%':
            kputc('%');
            break;
        default:
            kputc('%');
            kputc(*fmt);
            break;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
