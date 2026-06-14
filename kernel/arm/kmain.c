/* naos — B0 (ARM / AArch64): first "boot".
 * -----------------------------------------------------------------------------
 * Contrast with x86: the QEMU 'virt' machine has NO VGA video memory
 * (0xB8000). To display text, we write to the PL011 UART — a serial port
 * memory-mapped at 0x09000000. The output goes to the serial console (QEMU's
 * stdout), not to a screen: the check is read, it is not captured.
 * See docs/howto/00-setup.md §0.7. */

/* Data register (DR, offset 0) of the PL011 UART: writing to it emits a character.
 * Under QEMU, the UART is already ready to use — no init needed in B0. */
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
