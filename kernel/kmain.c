/* naos — C entry point of the kernel.
 *  B2–B6: Multiboot/VGA/GDT/IDT/IRQs.   B7: frames.   B8: paging.
 *  B9: kernel heap — kmalloc/kfree over a free-list arena.
 * See docs/howto/09-heap.md. */
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include <stdint.h>

static void put_hex(uint32_t v)
{
    vga_write("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int d = (v >> i) & 0xF;
        vga_putchar(d < 10 ? (char)('0' + d) : (char)('a' + d - 10));
    }
}

void kmain(uint32_t magic, uint32_t mb_info)
{
    (void)magic;
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B9: kernel heap (kmalloc / kfree)\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    gdt_init();
    idt_init();
    pic_remap();
    timer_init(100);
    keyboard_init();
    pmm_init((const struct multiboot_info *)mb_info);
    paging_init();
    heap_init();
    __asm__ volatile ("sti");

    char *a = (char *)kmalloc(16);
    void *b = kmalloc(100);
    void *c = kmalloc(4);
    vga_write("a = kmalloc(16)  = "); put_hex((uint32_t)a); vga_putchar('\n');
    vga_write("b = kmalloc(100) = "); put_hex((uint32_t)b); vga_putchar('\n');
    vga_write("c = kmalloc(4)   = "); put_hex((uint32_t)c); vga_putchar('\n');

    /* Write/read test: fill a with 'A'..'P', read it back. */
    for (int i = 0; i < 16; i++) a[i] = (char)('A' + i);
    char buf[17];
    for (int i = 0; i < 16; i++) buf[i] = a[i];
    buf[16] = '\0';
    vga_write("wrote 16 bytes to a, read back: ");
    vga_set_color(VGA_WHITE, VGA_BLACK); vga_write(buf);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK); vga_putchar('\n');

    /* Free b, then allocate again: first-fit should reuse b's slot. */
    vga_write("kfree(b)\n");
    kfree(b);
    void *d = kmalloc(50);
    vga_write("d = kmalloc(50)  = "); put_hex((uint32_t)d);
    vga_write("   reuses b? ");
    vga_set_color(d == b ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    vga_write(d == b ? "YES" : "no");

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("\n\nnaos B9: dynamic allocation works.");

    for (;;)
        __asm__ volatile ("hlt");
}
