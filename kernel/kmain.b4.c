/* naos — C entry point of the kernel.
 *  B2: prove kmain() runs (loaded by GRUB via Multiboot).
 *  B3: VGA driver (formatted text + colors + scrolling).
 *  B4: install a kernel-owned GDT.
 * Called by boot/boot.asm (_start). See docs/howto/04-gdt.md. */
#include "vga.h"
#include "gdt.h"

void kmain(void)
{
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B4: kernel booting...\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Installing the GDT (null, kernel code/data, user code/data)... ");
    gdt_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("done.\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Segments reloaded (CS=0x08, DS=0x10) via far jump, no triple fault:\n");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("the kernel now runs on its own GDT.");

    for (;;)
        __asm__ volatile ("hlt");
}
