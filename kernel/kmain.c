/* naos — C entry point of the kernel.
 *  B2–B6: Multiboot/VGA/GDT/IDT/IRQs.   B7: physical frame allocator.
 *  B8: paging — identity-map the low 4 MB + a higher-half window, enable CR0.PG,
 *      and prove virtual addressing by writing the screen through 0xC00B8000.
 * See docs/howto/08-paging.md. */
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "paging.h"
#include <stdint.h>

void kmain(uint32_t magic, uint32_t mb_info)
{
    (void)magic;
    vga_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B8: paging (virtual memory)\n\n");

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    gdt_init();
    idt_init();
    pic_remap();
    timer_init(100);
    keyboard_init();
    pmm_init((const struct multiboot_info *)mb_info);
    __asm__ volatile ("sti");

    vga_write("Enabling paging: identity-map 0-4MB + higher-half 0xC0000000 -> 0...\n");
    paging_init();
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("Paging on (CR0.PG=1) - the kernel still runs (identity map).\n\n");

    /* Prove virtual addressing: write to the screen through the HIGHER-HALF alias.
     * 0xC00B8000 is mapped to physical 0xB8000 (VGA), so this text appears even
     * though we never touch the physical address directly. */
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_write("Now writing the next line via VIRTUAL address 0xC00B8000:\n");

    const char *msg = "  [higher-half] this text reached VGA through 0xC00B8000  ";
    volatile uint16_t *hh = (volatile uint16_t *)0xC00B8000;   /* higher-half alias of 0xB8000 */
    uint32_t base = 12 * 80;                                    /* row 12 (clear of the text flow) */
    for (uint32_t i = 0; msg[i]; i++)
        hh[base + i] = (uint16_t)(uint8_t)msg[i] | (uint16_t)(0x0D << 8);   /* light magenta */

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_write("naos B8: virtual addressing + higher-half work (magenta line below).");

    for (;;)
        __asm__ volatile ("hlt");
}
