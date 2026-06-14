/* naos — B5: IDT setup. See include/idt.h and docs/howto/05-idt.md. */
#include "idt.h"
#include <stdint.h>

/* One IDT entry: a gate descriptor pointing at a handler. Packed to match HW. */
struct idt_entry {
    uint16_t base_low;    /* handler address [0:15]            */
    uint16_t selector;    /* code segment selector (0x08)      */
    uint8_t  zero;        /* always 0                          */
    uint8_t  flags;       /* P, DPL, gate type                 */
    uint16_t base_high;   /* handler address [16:31]           */
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];   /* zero-initialized (.bss) → vectors absent by default */
static struct idt_ptr   ip;

extern void idt_flush(uint32_t idt_ptr_addr);   /* lidt, in isr_stubs.asm */

/* The 32 CPU-exception stubs defined in kernel/isr_stubs.asm. */
extern void isr0(void),  isr1(void),  isr2(void),  isr3(void),  isr4(void),  isr5(void),
            isr6(void),  isr7(void),  isr8(void),  isr9(void),  isr10(void), isr11(void),
            isr12(void), isr13(void), isr14(void), isr15(void), isr16(void), isr17(void),
            isr18(void), isr19(void), isr20(void), isr21(void), isr22(void), isr23(void),
            isr24(void), isr25(void), isr26(void), isr27(void), isr28(void), isr29(void),
            isr30(void), isr31(void);

static void set_gate(int n, uint32_t handler)
{
    idt[n].base_low  = handler & 0xFFFF;
    idt[n].selector  = 0x08;     /* kernel code segment (from the GDT, B4) */
    idt[n].zero      = 0;
    idt[n].flags     = 0x8E;     /* P=1, DPL=0, type=0xE (32-bit interrupt gate) */
    idt[n].base_high = (handler >> 16) & 0xFFFF;
}

void idt_init(void)
{
    void (*stubs[32])(void) = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    };
    for (int i = 0; i < 32; i++)
        set_gate(i, (uint32_t)stubs[i]);

    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;
    idt_flush((uint32_t)&ip);
}
