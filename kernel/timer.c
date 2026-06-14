/* naos — B6: PIT timer on IRQ0. See include/timer.h and docs/howto/06-keyboard-timer.md. */
#include "timer.h"
#include "irq.h"
#include "ports.h"

#define PIT_FREQ    1193182u   /* base oscillator frequency (Hz) */
#define PIT_CH0     0x40
#define PIT_CMD     0x43

static volatile uint32_t ticks = 0;

static void on_tick(struct registers *r)
{
    (void)r;
    ticks++;
}

void timer_init(uint32_t hz)
{
    uint32_t divisor = PIT_FREQ / hz;
    outb(PIT_CMD, 0x36);                       /* channel 0, lo/hi byte, mode 3 (square wave) */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0, on_tick);
}

uint32_t timer_ticks(void)
{
    return ticks;
}
