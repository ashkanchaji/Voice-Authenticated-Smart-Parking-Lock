#include "config.h"
#include "systick.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

/* Owned by Timer0. No other module touches TCCR0A, TCCR0B, OCR0A or TIMSK0. */
static volatile uint32_t s_ms;

void systick_init(void)
{
    TCCR0A = _BV(WGM01);              /* CTC, OCR0A is TOP */
    TCCR0B = _BV(CS01) | _BV(CS00);   /* prescaler 64 */
    OCR0A = 249;                      /* 16e6 / (64 * 250) = 1000 Hz */
    TCNT0 = 0;
    TIMSK0 = _BV(OCIE0A);
    s_ms = 0;
}

uint32_t systick_ms(void)
{
    uint32_t v;
    /* A 32-bit read is four instructions on an 8-bit core; without this guard
     * the tick interrupt could land between two of them and hand back a value
     * that never existed. */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        v = s_ms;
    }
    return v;
}

ISR(TIMER0_COMPA_vect)
{
    ++s_ms;
}
