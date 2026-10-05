#include "config.h"
#include "systick.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/* TODO(timers): Timer0 as the 1 ms system timebase.
 *
 * Target: CTC mode with OCR0A as TOP, prescaler 64, OCR0A = 249, which gives
 * 16 MHz / (64 * 250) = 1000 Hz. Registers you will need: TCCR0A, TCCR0B,
 * OCR0A, TCNT0, TIMSK0.
 *
 * Then write ISR(TIMER0_COMPA_vect) to increment the counter below.
 *
 * Two things to get right:
 *   - The counter is shared between the interrupt and the main loop, so it
 *     needs `volatile`. Without it the compiler is entitled to cache it in a
 *     register and your timeouts will never expire.
 *   - It is 32 bits on an 8-bit core, so reading it takes four instructions.
 *     If the interrupt lands between two of them you get a value that never
 *     existed - a low half from before the increment and a high half from
 *     after. ATOMIC_BLOCK(ATOMIC_RESTORESTATE) from <util/atomic.h> is the fix.
 *     Work out for yourself how often that actually bites (hint: consider what
 *     happens at millisecond 65536) and why "it works on my machine" is not
 *     evidence that it does not.
 */

static volatile uint32_t s_ms;

void systick_init(void)
{
    /* TODO: configure Timer0 for a 1 ms CTC tick and enable OCIE0A. */
    s_ms = 0;
}

uint32_t systick_ms(void)
{
    /* TODO: return the tick counter, read atomically. */
    return 0;
}

/* TODO: ISR(TIMER0_COMPA_vect) { ++s_ms; } */
