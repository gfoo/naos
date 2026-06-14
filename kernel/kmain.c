/* naos — point d'entrée C du kernel.
 *  B2 : prouver que kmain() s'exécute (chargé par GRUB via Multiboot).
 *  B3 : utiliser le driver VGA (texte formaté + couleurs + défilement).
 * Appelé par boot/boot.asm (_start). Voir docs/HOWTO.md §2 et §3. */
#include "vga.h"

/* mini-itoa pour entiers >= 0 (suffisant pour la démo de scroll). */
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

    /* Preuve du défilement : imprimer plus de 25 lignes force le scroll. */
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
