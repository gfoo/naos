/* naos — B0 (ARM / AArch64) : premier « boot ».
 * -----------------------------------------------------------------------------
 * Contraste avec le x86 : la machine QEMU 'virt' n'a PAS de mémoire vidéo VGA
 * (0xB8000). Pour afficher du texte, on écrit dans l'UART PL011 — un port série
 * mappé en mémoire à 0x09000000. La sortie part sur la console série (stdout de
 * QEMU), pas sur un écran : la vérif se lit, elle ne se capture pas.
 * Voir docs/howto/00-setup.md §0.7. */

/* Registre de données (DR, offset 0) de l'UART PL011 : y écrire émet un caractère.
 * Sous QEMU, l'UART est déjà prêt à l'emploi — aucune init nécessaire en B0. */
static volatile unsigned int *const UART0_DR = (unsigned int *)0x09000000;

static void uart_putc(char c)
{
    *UART0_DR = (unsigned int)(unsigned char)c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

void kmain(void)
{
    uart_puts("naos B0 (ARM): it boots!\n");
    for (;;)
        __asm__ volatile ("wfe");
}
