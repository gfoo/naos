/* naos — B6: x86 port I/O (in/out instructions, which C can't express).
 * See docs/howto/06-keyboard-timer.md. */
#ifndef NAOS_PORTS_H
#define NAOS_PORTS_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* A short, harmless I/O to give old hardware a moment to settle. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* NAOS_PORTS_H */
