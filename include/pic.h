/* naos — B6: the 8259 PIC (Programmable Interrupt Controller).
 * See docs/howto/06-keyboard-timer.md. */
#ifndef NAOS_PIC_H
#define NAOS_PIC_H

#include <stdint.h>

void pic_remap(void);             /* move IRQ 0..15 to IDT vectors 32..47 */
void pic_send_eoi(uint8_t irq);   /* signal end-of-interrupt to the PIC(s) */

#endif /* NAOS_PIC_H */
