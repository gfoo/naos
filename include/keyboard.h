/* naos — B6: PS/2 keyboard on IRQ1. See docs/howto/06-keyboard-timer.md. */
#ifndef NAOS_KEYBOARD_H
#define NAOS_KEYBOARD_H

void keyboard_init(void);   /* install the IRQ1 handler */

#endif /* NAOS_KEYBOARD_H */
