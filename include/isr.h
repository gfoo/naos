/* naos — B5: interrupt service routines (CPU exceptions).
 * The ISR stubs (kernel/isr_stubs.asm) save the CPU state on the stack in this
 * exact order, then call isr_handler() with a pointer to it. See docs/howto/05-idt.md. */
#ifndef NAOS_ISR_H
#define NAOS_ISR_H

#include <stdint.h>

/* CPU state captured by the ISR stubs, laid out in stack order (low → high). */
struct registers {
    uint32_t ds;                                       /* saved data segment            */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* pushed by `pusha`             */
    uint32_t int_no, err_code;                         /* pushed by the stub            */
    uint32_t eip, cs, eflags, useresp, ss;             /* pushed by the CPU on interrupt */
};

void isr_handler(struct registers *r);   /* C dispatcher, called from isr_common */

#endif /* NAOS_ISR_H */
