/* naos — B3: VGA text-mode screen driver (0xB8000 buffer, 80x25).
 * Each cell = 2 bytes: character (CP437) + color attribute (4 bits background,
 * 4 bits foreground). See docs/howto/03-vga.md. */
#ifndef NAOS_VGA_H
#define NAOS_VGA_H

#include <stdint.h>
#include <stddef.h>

/* The 16 VGA colors (standard text palette). */
enum vga_color {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_LIGHT_BROWN, VGA_WHITE,
};

void vga_init(void);                                   /* clears the screen, cursor (0,0) */
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_putchar(char c);                              /* handles \n \r \t \b + scroll */
void vga_write(const char *s);                         /* writes a C string */
void vga_write_at(size_t row, size_t col, const char *s);  /* write at (row,col), cursor unchanged */

#endif /* NAOS_VGA_H */
