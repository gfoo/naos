/* naos — B6: PIT (Programmable Interval Timer) on IRQ0. See docs/howto/06-keyboard-timer.md. */
#ifndef NAOS_TIMER_H
#define NAOS_TIMER_H

#include <stdint.h>

void     timer_init(uint32_t hz);   /* program the PIT and install the IRQ0 handler */
uint32_t timer_ticks(void);         /* ticks elapsed since timer_init */

#endif /* NAOS_TIMER_H */
