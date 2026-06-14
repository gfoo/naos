/* naos — B3 : driver écran VGA mode texte. Voir include/vga.h et docs/howto/03-vga.md. */
#include "vga.h"

#define VGA_MEM  ((volatile uint16_t *)0xB8000)
#define COLS     80
#define ROWS     25

static size_t  row, col;     /* curseur logique */
static uint8_t color;        /* attribut courant (fond<<4 | avant-plan) */

/* Une cellule VGA = caractère (octet bas) + attribut (octet haut). */
static inline uint16_t cell(char c, uint8_t attr)
{
    return (uint16_t)(unsigned char)c | ((uint16_t)attr << 8);
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    color = (uint8_t)fg | (uint8_t)(bg << 4);
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (size_t y = 0; y < ROWS; y++)
        for (size_t x = 0; x < COLS; x++)
            VGA_MEM[y * COLS + x] = cell(' ', color);
    row = col = 0;
}

/* Remonte tout d'une ligne et vide la dernière ; le curseur reste en bas. */
static void scroll(void)
{
    for (size_t y = 1; y < ROWS; y++)
        for (size_t x = 0; x < COLS; x++)
            VGA_MEM[(y - 1) * COLS + x] = VGA_MEM[y * COLS + x];
    for (size_t x = 0; x < COLS; x++)
        VGA_MEM[(ROWS - 1) * COLS + x] = cell(' ', color);
    row = ROWS - 1;
}

void vga_putchar(char c)
{
    switch (c) {
    case '\n': col = 0; row++; break;
    case '\r': col = 0; break;
    case '\b': if (col) col--; break;
    case '\t': col = (col + 8) & ~(size_t)7; break;
    default:
        VGA_MEM[row * COLS + col] = cell(c, color);
        col++;
    }
    if (col >= COLS) { col = 0; row++; }
    if (row >= ROWS) scroll();
}

void vga_write(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}
