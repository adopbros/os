/*
 * Arrowix OS - PS/2 keyboard driver (IRQ1).
 */
#pragma once

#include <arrowix/types.h>

/*
 * Special key codes returned by keyboard_getchar() alongside ASCII. ESC keeps
 * its natural ASCII value; the function keys use codes outside printable ASCII
 * so the shell can tell them apart from typed characters.
 */
#define KEY_ESC 0x1B
#define KEY_F1  0x90
#define KEY_F2  0x91
#define KEY_F3  0x92

#ifdef __cplusplus
extern "C" {
#endif

/* Register the IRQ1 handler and drain the controller. */
void keyboard_init(void);

/*
 * Non-blocking read of the next decoded key, or 0 if the input buffer is empty.
 * Returns ASCII for printable keys (plus '\n', '\b', '\t') and the KEY_* codes
 * above for ESC/F1-F3. The driver does not echo; the shell controls display.
 */
int keyboard_getchar(void);

#ifdef __cplusplus
}
#endif
