/* naos — C entry point of the kernel.
 *  B2: prove kmain() runs (loaded by GRUB via Multiboot).
 *  B3: VGA driver (formatted text + colors + scrolling).
 *  B4: install a kernel-owned GDT.
 *  B5: install the IDT and handle CPU exceptions (divide-by-zero demo).
 * Called by boot/boot.asm (_start). See docs/howto/05-idt.md. */
#include "vga.h"
#include "gdt.h"
#include "idt.h"

void kmain(void)
{
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B5: kernel booting...\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("GDT (kernel + user segments)... ");
    gdt_init();
    vga_write("ok.\n");
    vga_write("IDT (256 gates, 32 CPU-exception handlers)... ");
    idt_init();
    vga_write("ok.\n\n");

    vga_write("Triggering a divide-by-zero to exercise the handler:\n");
    volatile int zero = 0;
    volatile int x = 42 / zero;   /* #DE → isr0 → isr_handler() */
    (void)x;

    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_write("\n(unreachable: the exception handler should have run)");
    for (;;)
        __asm__ volatile ("hlt");
}
