/* naos — C entry point of the kernel.
 *  B2: prove kmain() runs (loaded by GRUB via Multiboot).
 *  B3: VGA driver.   B4: GDT.   B5: IDT + exceptions.   B6: timer + keyboard IRQs.
 *  B7: physical memory — parse the Multiboot map, allocate/free physical frames.
 * Called by boot/boot.asm (_start), which passes the Multiboot magic + info pointer.
 * See docs/howto/07-pmm.md. */
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"

static void put_dec(uint32_t n)
{
    char b[11]; int i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n) { b[i++] = (char)('0' + n % 10); n /= 10; }
    while (i--) vga_putchar(b[i]);
}

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
    vga_write("naos B7: physical memory\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    gdt_init();
    idt_init();
    pic_remap();
    timer_init(100);
    keyboard_init();
    __asm__ volatile ("sti");

    pmm_init((const struct multiboot_info *)mb_info);
    vga_write("Multiboot memory map parsed. Usable frames: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    put_dec(pmm_total_count());
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write(" (");
    put_dec(pmm_total_count() * 4);
    vga_write(" KB)\n\n");

    vga_write("alloc a = "); uint32_t a = pmm_alloc_frame(); put_hex(a); vga_putchar('\n');
    vga_write("alloc b = "); uint32_t b = pmm_alloc_frame(); put_hex(b); vga_putchar('\n');
    vga_write("free  a\n"); pmm_free_frame(a);
    vga_write("alloc c = "); uint32_t c = pmm_alloc_frame(); put_hex(c);
    vga_write("   reuses a? ");
    vga_set_color(c == a ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    vga_write(c == a ? "YES" : "no");

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("\n\nnaos B7: frame allocator works.");

    for (;;)
        __asm__ volatile ("hlt");
}
