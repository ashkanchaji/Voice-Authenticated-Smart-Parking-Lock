#include "config.h"
#include "timer2.h"
#include "adc_sampler.h"
#include "dac_audio.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/* TODO(timers): Timer2 as the shared 8 kHz audio clock.
 *
 * Target: CTC mode with OCR2A as TOP, prescaler 8, OCR2A = TIMER2_OCR_VALUE,
 * which gives 16 MHz / (8 * 250) = 8000 Hz. Registers: TCCR2A, TCCR2B, OCR2A,
 * TCNT2, TIMSK2.
 *
 * Careful: Timer2's prescaler encoding is NOT the same as Timer0's and
 * Timer1's. Timer2 has /32 and /128 options the others lack, so the CS2[2:0]
 * bit patterns are shifted. Check the datasheet table rather than copying the
 * bits from your Timer0 code.
 *
 * There can be only one ISR(TIMER2_COMPA_vect) in the program, and two
 * different jobs need it, so this module owns the vector and dispatches on the
 * current mode:
 *
 *   SAMPLING  -> adc_trigger_conversion()   (adc_sampler.cpp)
 *   PLAYBACK  -> dac_audio_tick()           (dac_audio.cpp)
 *
 * Keep the handler to a handful of instructions. At 8 kHz there are only 2000
 * CPU cycles between ticks and the main loop needs nearly all of them for the
 * FFT.
 */

static volatile Timer2Mode s_mode = Timer2Mode::OFF;

void timer2_init(void)
{
    /* TODO: put Timer2 in CTC mode with OCR2A = TIMER2_OCR_VALUE, clock
     * stopped, interrupt disabled. */
    s_mode = Timer2Mode::OFF;
}

Timer2Mode timer2_mode(void)
{
    return s_mode;
}

void timer2_set_mode(Timer2Mode mode)
{
    /* TODO: disable the interrupt and stop the clock before changing anything,
     * then restart both unless the new mode is OFF. */
    s_mode = mode;
}

void timer2_stop_isr(void)
{
    /* TODO: same as timer2_set_mode(Timer2Mode::OFF), safe to call from an ISR. */
    s_mode = Timer2Mode::OFF;
}

/* TODO: ISR(TIMER2_COMPA_vect) dispatching on s_mode. */
