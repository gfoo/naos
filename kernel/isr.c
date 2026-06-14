/* naos — B5: C dispatcher for CPU exceptions. See include/isr.h and docs/howto/05-idt.md. */
#include "isr.h"
#include "vga.h"

static const char *exc_name[32] = {
    "Divide-by-zero",          "Debug",                 "Non-maskable interrupt", "Breakpoint",
    "Overflow",                "Bound range exceeded",  "Invalid opcode",         "Device not available",
    "Double fault",            "Coprocessor overrun",   "Invalid TSS",            "Segment not present",
    "Stack-segment fault",     "General protection",    "Page fault",             "Reserved",
    "x87 FP exception",        "Alignment check",       "Machine check",          "SIMD FP exception",
    "Virtualization",          "Control protection",    "Reserved",               "Reserved",
    "Reserved",                "Reserved",              "Reserved",               "Reserved",
    "Hypervisor injection",    "VMM communication",     "Security",               "Reserved",
};

static void put_dec(uint32_t n)
{
    char b[11];
    int i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n) { b[i++] = (char)('0' + n % 10); n /= 10; }
    while (i--) vga_putchar(b[i]);
}

void isr_handler(struct registers *r)
{
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_write("\n[CPU EXCEPTION] #");
    put_dec(r->int_no);
    vga_write("  ");
    if (r->int_no < 32)
        vga_write(exc_name[r->int_no]);
    vga_write("\nKernel halted.");

    /* A CPU fault re-runs the faulting instruction on iret → we'd loop forever.
     * For B5, an exception is fatal: panic and stop. (B6 will make IRQs return.) */
    for (;;)
        __asm__ volatile ("cli; hlt");
}
