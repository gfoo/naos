/* naos — B5: IDT (Interrupt Descriptor Table).
 * Maps interrupt/exception vectors to handler addresses. See docs/howto/05-idt.md. */
#ifndef NAOS_IDT_H
#define NAOS_IDT_H

void idt_init(void);   /* fill the IDT (CPU exceptions 0..31) and load it (lidt) */

#endif /* NAOS_IDT_H */
