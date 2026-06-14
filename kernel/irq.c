/* naos — B6: IRQ dispatcher. See include/irq.h and docs/howto/06-keyboard-timer.md. */
#include "irq.h"
#include "pic.h"

static irq_handler_t handlers[16] = { 0 };

void irq_install_handler(int irq, irq_handler_t handler)
{
    if (irq >= 0 && irq < 16)
        handlers[irq] = handler;
}

/* Called from irq_common (kernel/irq_stubs.asm) for vectors 32..47. */
void irq_handler(struct registers *r)
{
    int irq = (int)r->int_no - 32;
    if (irq >= 0 && irq < 16 && handlers[irq])
        handlers[irq](r);
    pic_send_eoi((uint8_t)irq);   /* always acknowledge, even with no handler */
}
