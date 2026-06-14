/* naos — B6: remap the 8259 PIC. See include/pic.h and docs/howto/06-keyboard-timer.md. */
#include "pic.h"
#include "ports.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x11   /* begin init, expect ICW4 */
#define ICW4_8086 0x01   /* 8086/88 mode */
#define PIC_EOI   0x20   /* end-of-interrupt command */

/* By default the PIC delivers IRQ 0..7 on vectors 8..15 — which collide with the
 * CPU exceptions (e.g. IRQ0 timer would look like #DF). We remap to 32..47. */
void pic_remap(void)
{
    outb(PIC1_CMD, ICW1_INIT); io_wait();
    outb(PIC2_CMD, ICW1_INIT); io_wait();
    outb(PIC1_DATA, 0x20);     io_wait();   /* master vector offset = 32 */
    outb(PIC2_DATA, 0x28);     io_wait();   /* slave vector offset  = 40 */
    outb(PIC1_DATA, 0x04);     io_wait();   /* tell master: slave on IRQ2 */
    outb(PIC2_DATA, 0x02);     io_wait();   /* tell slave its cascade identity */
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();

    outb(PIC1_DATA, 0x00);                  /* unmask all IRQs on master */
    outb(PIC2_DATA, 0x00);                  /* unmask all IRQs on slave  */
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);   /* slave too, for IRQ 8..15 */
    outb(PIC1_CMD, PIC_EOI);
}
