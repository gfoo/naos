/* naos — B6: PS/2 keyboard driver on IRQ1.
 * Reads scancodes (set 1) from port 0x60 and echoes the character to the screen.
 * See include/keyboard.h and docs/howto/06-keyboard-timer.md. */
#include "keyboard.h"
#include "irq.h"
#include "ports.h"
#include "vga.h"

#define KBD_DATA 0x60

/* Scancode set 1 → ASCII, lowercase, no modifiers. Unmapped keys stay 0. */
static const char scancode_ascii[128] = {
    [0x02]='1', [0x03]='2', [0x04]='3', [0x05]='4', [0x06]='5',
    [0x07]='6', [0x08]='7', [0x09]='8', [0x0A]='9', [0x0B]='0',
    [0x0C]='-', [0x0D]='=', [0x0E]='\b', [0x0F]='\t',
    [0x10]='q', [0x11]='w', [0x12]='e', [0x13]='r', [0x14]='t',
    [0x15]='y', [0x16]='u', [0x17]='i', [0x18]='o', [0x19]='p',
    [0x1A]='[', [0x1B]=']', [0x1C]='\n',
    [0x1E]='a', [0x1F]='s', [0x20]='d', [0x21]='f', [0x22]='g',
    [0x23]='h', [0x24]='j', [0x25]='k', [0x26]='l', [0x27]=';',
    [0x28]='\'',[0x29]='`', [0x2B]='\\',
    [0x2C]='z', [0x2D]='x', [0x2E]='c', [0x2F]='v', [0x30]='b',
    [0x31]='n', [0x32]='m', [0x33]=',', [0x34]='.', [0x35]='/',
    [0x39]=' ',
};

static void on_key(struct registers *r)
{
    (void)r;
    uint8_t sc = inb(KBD_DATA);
    if (sc & 0x80)            /* high bit set = key release: ignore */
        return;
    char c = scancode_ascii[sc & 0x7F];
    if (c)
        vga_putchar(c);
}

void keyboard_init(void)
{
    irq_install_handler(1, on_key);
}
