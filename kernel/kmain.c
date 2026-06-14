/* naos — C entry point of the kernel.
 *  B2: prove that kmain() executes (loaded by GRUB via Multiboot).
 *  B3: use the VGA driver (formatted text + colors + scrolling).
 * Called by boot/boot.asm (_start). See docs/howto/02-multiboot.md and 03-vga.md. */
#include "vga.h"

/* mini-itoa for integers >= 0 (sufficient for the scroll demo). */
static void put_uint(unsigned int n)
{
    char buf[11];
    int i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    while (i--) vga_putchar(buf[i]);
}

void kmain(void)
{
    vga_init();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B3: VGA driver online.\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Booted by GRUB via Multiboot; kmain() runs in 32-bit C.\n\n");

    /* Scrolling proof: printing more than 25 lines forces a scroll. */
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    for (unsigned int i = 1; i <= 30; i++) {
        vga_write("  line ");
        put_uint(i);
        vga_putchar('\n');
    }

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("\nnaos B3: scroll OK.");

    for (;;)
        __asm__ volatile ("hlt");
}
