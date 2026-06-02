/*
 * Arrowix OS - PS/2 keyboard driver (IRQ1, scancode Set 1).
 *
 * The 8042 controller raises IRQ1 whenever a key is pressed or released and
 * places a scancode in the data port (0x60). We translate Set 1 make codes to
 * ASCII (honoring Shift and Caps Lock) plus a few special keys (ESC, F1-F3) and
 * queue them in a small ring buffer the shell consumes via keyboard_getchar().
 * The driver itself does not echo; the shell decides what to display and where.
 */

#include <arrowix/keyboard.h>
#include <arrowix/io.h>
#include <arrowix/irq.h>
#include <arrowix/isr.h>

#define PS2_DATA   0x60
#define PS2_STATUS 0x64

/* Status register bit 0: output buffer full (data available to read). */
#define PS2_STATUS_OBF 0x01

/* Set 1 make codes for the modifier keys we track. */
#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CAPS   0x3A
#define SC_EXTEND 0xE0

/* Set 1 make codes for special keys we surface to the shell. */
#define SC_ESC 0x01
#define SC_F1  0x3B
#define SC_F2  0x3C
#define SC_F3  0x3D

/* Scancode Set 1 -> ASCII (unshifted). 0 means "no echo / unsupported". */
static const char kScancodeAscii[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4', [0x06] = '5',
    [0x07] = '6', [0x08] = '7', [0x09] = '8', [0x0A] = '9', [0x0B] = '0',
    [0x0C] = '-', [0x0D] = '=', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r', [0x14] = 't',
    [0x15] = 'y', [0x16] = 'u', [0x17] = 'i', [0x18] = 'o', [0x19] = 'p',
    [0x1A] = '[', [0x1B] = ']', [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f', [0x22] = 'g',
    [0x23] = 'h', [0x24] = 'j', [0x25] = 'k', [0x26] = 'l', [0x27] = ';',
    [0x28] = '\'', [0x29] = '`', [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v', [0x30] = 'b',
    [0x31] = 'n', [0x32] = 'm', [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*', [0x39] = ' ',
};

/* Scancode Set 1 -> ASCII (with Shift held). */
static const char kScancodeAsciiShift[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$', [0x06] = '%',
    [0x07] = '^', [0x08] = '&', [0x09] = '*', [0x0A] = '(', [0x0B] = ')',
    [0x0C] = '_', [0x0D] = '+', [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R', [0x14] = 'T',
    [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I', [0x18] = 'O', [0x19] = 'P',
    [0x1A] = '{', [0x1B] = '}', [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F', [0x22] = 'G',
    [0x23] = 'H', [0x24] = 'J', [0x25] = 'K', [0x26] = 'L', [0x27] = ':',
    [0x28] = '"', [0x29] = '~', [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V', [0x30] = 'B',
    [0x31] = 'N', [0x32] = 'M', [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*', [0x39] = ' ',
};

static bool g_shift;
static bool g_caps;
static bool g_extended; /* previous byte was the 0xE0 prefix */

/* Ring buffer of decoded key codes (producer: IRQ; consumer: getchar). */
#define KBD_BUFFER_SIZE 128
static volatile u8 g_buffer[KBD_BUFFER_SIZE];
static volatile u32 g_head;
static volatile u32 g_tail;

static void buffer_put(u8 c)
{
    u32 next = (g_head + 1) % KBD_BUFFER_SIZE;
    if (next != g_tail) { /* drop on overflow */
        g_buffer[g_head] = c;
        g_head = next;
    }
}

static char translate(u8 scancode)
{
    char base = kScancodeAscii[scancode];
    if (base == 0) {
        return 0;
    }

    /* Letters are affected by both Shift and Caps Lock (XOR); everything else
     * only by Shift. */
    if (base >= 'a' && base <= 'z') {
        bool upper = g_shift ^ g_caps;
        return upper ? (char) (base - 'a' + 'A') : base;
    }
    return g_shift ? kScancodeAsciiShift[scancode] : base;
}

static void kbd_irq_handler(struct registers *r)
{
    (void) r;

    u8 scancode = inb(PS2_DATA);

    /* Swallow the byte following an 0xE0 prefix (arrows, etc.). */
    if (scancode == SC_EXTEND) {
        g_extended = true;
        return;
    }
    if (g_extended) {
        g_extended = false;
        return;
    }

    /* Break code (key released): only modifiers care. */
    if (scancode & 0x80) {
        u8 make = scancode & 0x7F;
        if (make == SC_LSHIFT || make == SC_RSHIFT) {
            g_shift = false;
        }
        return;
    }

    /* Make code. */
    switch (scancode) {
    case SC_LSHIFT:
    case SC_RSHIFT:
        g_shift = true;
        return;
    case SC_CAPS:
        g_caps = !g_caps;
        return;
    case SC_ESC:
        buffer_put(KEY_ESC);
        return;
    case SC_F1:
        buffer_put(KEY_F1);
        return;
    case SC_F2:
        buffer_put(KEY_F2);
        return;
    case SC_F3:
        buffer_put(KEY_F3);
        return;
    default:
        break;
    }

    char ch = translate(scancode);
    if (ch != 0) {
        buffer_put((u8) ch);
    }
}

int keyboard_getchar(void)
{
    if (g_tail == g_head) {
        return 0;
    }
    u8 c = g_buffer[g_tail];
    g_tail = (g_tail + 1) % KBD_BUFFER_SIZE;
    return (int) c;
}

void keyboard_init(void)
{
    /* Drain any bytes the controller latched before we installed the handler. */
    while (inb(PS2_STATUS) & PS2_STATUS_OBF) {
        (void) inb(PS2_DATA);
    }

    irq_register_handler(1, kbd_irq_handler);
}
