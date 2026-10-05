#include "config.h"
#include "timer2.h"
#include "adc_sampler.h"
#include "dac_audio.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile Timer2Mode s_mode = Timer2Mode::OFF;

void timer2_init(void)
{
    TCCR2A = _BV(WGM21);         /* CTC, OCR2A is TOP */
    TCCR2B = 0;                  /* clock stopped until a mode is selected */
    OCR2A = (uint8_t)TIMER2_OCR_VALUE;
    TCNT2 = 0;
    TIMSK2 = 0;
    s_mode = Timer2Mode::OFF;
}

Timer2Mode timer2_mode(void)
{
    return s_mode;
}

void timer2_set_mode(Timer2Mode mode)
{
    TIMSK2 = 0;   /* silence the interrupt before anything else changes */
    TCCR2B = 0;   /* stop the clock */
    s_mode = mode;
    if (mode == Timer2Mode::OFF) {
        return;
    }
    TCNT2 = 0;
    /* CS2[2:0] = 010 selects prescaler 8. Timer2's prescaler encoding is not
     * the same as Timer0/Timer1's - it has /32 and /128 options they lack, so
     * the bit patterns shift. */
    TCCR2B = _BV(CS21);
    TIMSK2 = _BV(OCIE2A);
}

void timer2_stop_isr(void)
{
    TIMSK2 = 0;
    TCCR2B = 0;
    s_mode = Timer2Mode::OFF;
}

ISR(TIMER2_COMPA_vect)
{
    /* Kept to a handful of instructions: at 8 kHz there are 2000 CPU cycles
     * between ticks and the main loop needs almost all of them for the FFT. */
    if (s_mode == Timer2Mode::SAMPLING) {
        adc_trigger_conversion();
    } else if (s_mode == Timer2Mode::PLAYBACK) {
        dac_audio_tick();
    }
}
