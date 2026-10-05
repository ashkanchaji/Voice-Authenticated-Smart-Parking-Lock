#include "config.h"
#include "pins.h"
#include "adc_sampler.h"
#include "timer2.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/** Midpoint of the 10-bit ADC range; the 2.5 V bias sits here. */
static const int16_t ADC_BIAS = 512;

static volatile int16_t s_buf[2][FRAME_LEN];
static volatile uint8_t s_fill;      /* buffer the interrupt is filling */
static volatile uint8_t s_index;     /* next slot inside that buffer */
static volatile uint8_t s_ready_buf; /* buffer handed to the main loop */
static volatile bool s_ready;        /* a frame is waiting */
static volatile uint16_t s_overrun;
static volatile bool s_active;

void adc_init(void)
{
    /* PC0 is analogue in: no pull-up, and its digital input buffer is turned
     * off so the switching input stage cannot inject noise into the sample. */
    DDRC &= (uint8_t)~_BV(PC0);
    PORTC &= (uint8_t)~_BV(PC0);
    DIDR0 |= _BV(ADC0D);

    ADMUX = _BV(REFS0) | (MIC_ADC_CHANNEL & 0x0Fu); /* AVcc reference, ADC0 */
    ADCSRB = 0;                                     /* free running source unused */
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); /* enable, /128 */

    /* Throwaway 25-clock conversion so the first real sample is on time. */
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
        /* wait */
    }
    (void)ADC;

    ADCSRA |= _BV(ADIF);  /* clear the flag the dummy conversion just set */
    ADCSRA |= _BV(ADIE);  /* from now on, completion raises ADC_vect */
}

void adc_sampling_start(void)
{
    ADCSRA |= _BV(ADIF);
    s_fill = 0;
    s_index = 0;
    s_ready = false;
    s_active = true;
    timer2_set_mode(Timer2Mode::SAMPLING);
}

void adc_sampling_stop(void)
{
    timer2_set_mode(Timer2Mode::OFF);
    s_active = false;
    s_ready = false;
    s_index = 0;
}

bool adc_sampling_active(void)
{
    return s_active;
}

bool adc_frame_ready(void)
{
    return s_ready;
}

const int16_t *adc_frame_get(void)
{
    return (const int16_t *)s_buf[s_ready_buf];
}

void adc_frame_release(void)
{
    s_ready = false;
}

uint16_t adc_overrun_count(void)
{
    uint16_t v;
    uint8_t sreg = SREG;
    cli();
    v = s_overrun;
    SREG = sreg;
    return v;
}

void adc_overrun_reset(void)
{
    uint8_t sreg = SREG;
    cli();
    s_overrun = 0;
    SREG = sreg;
}

void adc_trigger_conversion(void)
{
    ADCSRA |= _BV(ADSC);
}

ISR(ADC_vect)
{
    /* Read ADCL first: the hardware latches ADCH until ADCL is read, and the
     * `ADC` macro expands to that ordered 16-bit access. */
    int16_t sample = (int16_t)ADC - ADC_BIAS;

    s_buf[s_fill][s_index] = sample;
    if (++s_index < (uint8_t)FRAME_LEN) {
        return;
    }

    s_index = 0;
    if (s_ready) {
        /* The main loop still owns the other buffer. Dropping this frame and
         * refilling the same buffer is the only safe move - swapping now would
         * hand the interrupt the memory the main loop is reading. */
        ++s_overrun;
        return;
    }
    s_ready_buf = s_fill;
    s_ready = true;
    s_fill ^= 1u;
}
