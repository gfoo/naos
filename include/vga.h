/* naos — B3 : driver écran VGA mode texte (buffer 0xB8000, 80x25).
 * Chaque cellule = 2 octets : caractère (CP437) + attribut couleur (4 bits fond,
 * 4 bits avant-plan). Voir docs/HOWTO.md §3. */
#ifndef NAOS_VGA_H
#define NAOS_VGA_H

#include <stdint.h>
#include <stddef.h>

/* Les 16 couleurs VGA (palette texte standard). */
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_LIGHT_BROWN, VGA_WHITE,
};

void vga_init(void);                                   /* efface l'écran, curseur (0,0) */
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_putchar(char c);                              /* gère \n \r \t \b + scroll */
void vga_write(const char *s);                         /* écrit une chaîne C */

#endif /* NAOS_VGA_H */
