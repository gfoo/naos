/* naos — C entry point of the kernel.
 *  B2: prove kmain() runs (loaded by GRUB via Multiboot).
 *  B3: VGA driver (formatted text + colors + scrolling).
 *  B4: install a kernel-owned GDT.
 *  B5: install the IDT and handle CPU exceptions.
 *  B6: hardware interrupts — PIT timer (IRQ0) + PS/2 keyboard (IRQ1).
 * Called by boot/boot.asm (_start). See docs/howto/06-keyboard-timer.md. */
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"

/* Render "ticks: N" into buf (no libc). */
static void format_ticks(char *buf, uint32_t n)
{
    const char *p = "ticks: ";
    int i = 0;
    while (*p) buf[i++] = *p++;
    char d[11]; int j = 0;
    if (n == 0) d[j++] = '0';
    while (n) { d[j++] = (char)('0' + n % 10); n /= 10; }
    while (j--) buf[i++] = d[j];
    buf[i] = '\0';
}

void kmain(void)
{
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B6: keyboard + timer\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    gdt_init();
    idt_init();
    pic_remap();
    timer_init(100);        /* 100 Hz */
    keyboard_init();
    __asm__ volatile ("sti");   /* enable interrupts */

    vga_write("Interrupts on. PIT @100Hz (IRQ0) + PS/2 keyboard (IRQ1).\n");
    vga_write("Type on the keyboard (characters echo below); the tick counter\n");
    vga_write("runs top-right.\n\n");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_write("> ");

    /* Idle loop: woken by every interrupt (timer or key). On each tick change,
     * refresh the counter at a fixed position; keys echo at the normal cursor. */
    uint32_t last = 0xFFFFFFFF;
    for (;;) {
        uint32_t t = timer_ticks();
        if (t != last) {
            last = t;
            char buf[24];
            format_ticks(buf, t);
            vga_write_at(0, 60, buf);
        }
        __asm__ volatile ("hlt");
    }
}
