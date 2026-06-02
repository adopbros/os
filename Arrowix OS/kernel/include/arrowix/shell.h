/*
 * Arrowix OS - Interactive shell ("Arrowix Shell").
 *
 * Owns the presentation layer: a boot splash, a framed VGA UI (status bar,
 * central console, shortcut bar), a live PIT clock, and a small command
 * interpreter driven by the PS/2 keyboard ring buffer.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Play the boot splash, draw the UI, and enter the interactive command loop.
 * Never returns. Requires interrupts enabled (PIT + keyboard) beforehand.
 */
void shell_run(void);

/*
 * Refresh the persistent taskbar clock (row 24). Safe to call from the shell
 * main loop and from the PIT tick (IRQ0) callback; it only repaints when the
 * second changes and is a no-op while a full-screen app owns the screen.
 */
void update_taskbar(void);

#ifdef __cplusplus
}
#endif
