/* naos — B6: hardware IRQ dispatch (vectors 32..47). See docs/howto/06-keyboard-timer.md. */
#ifndef NAOS_IRQ_H
#define NAOS_IRQ_H

#include "isr.h"

typedef void (*irq_handler_t)(struct registers *r);

void irq_install_handler(int irq, irq_handler_t handler);  /* irq in 0..15 */
void irq_handler(struct registers *r);                     /* called from irq_common */

#endif /* NAOS_IRQ_H */
