#ifndef ADC_SAMPLER_H
#define ADC_SAMPLER_H

/* Register-level ADC acquisition into a ping-pong frame buffer.
 *
 * Timer2 fires at 8 kHz and starts a conversion; ADC_vect stores the result and
 * nothing else. When 64 samples have landed the frame is published to the main
 * loop and the interrupt continues into the other buffer, so the main loop has
 * a full 8 ms to run the FFT on the frame it was handed without racing the
 * interrupt that is filling the next one.
 *
 * analogRead() is not used anywhere: it busy-waits for the conversion, which
 * would burn 104 us of every 125 us period inside an interrupt.
 */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/** Configure ADMUX/ADCSRA and run one throwaway conversion.
 *
 * AVcc reference, ADC0, prescaler 128 -> 16 MHz / 128 = 125 kHz ADC clock.
 * A normal conversion is 13 ADC clocks = 104 us, which fits inside the 125 us
 * sample period. The first conversion after enabling the ADC takes 25 clocks
 * (200 us) because it also initialises the analogue core, so it is done here,
 * once, rather than being allowed to overrun the first sample period.
 */
void adc_init(void);

/** Start sampling: clears the buffers and puts Timer2 into SAMPLING mode. */
void adc_sampling_start(void);

/** Stop sampling and leave Timer2 idle. */
void adc_sampling_stop(void);

/** True while sampling is running. */
bool adc_sampling_active(void);

/** True when a complete frame is waiting to be processed. */
bool adc_frame_ready(void);

/** Pointer to the completed frame. Valid until adc_frame_release(). */
const int16_t *adc_frame_get(void);

/** Hand the frame back so the interrupt may reuse that buffer. */
void adc_frame_release(void);

/** Frames dropped because the main loop did not keep up. Should stay 0. */
uint16_t adc_overrun_count(void);

/** Clear the overrun counter. */
void adc_overrun_reset(void);

/** Called from the Timer2 interrupt in SAMPLING mode. Starts one conversion. */
void adc_trigger_conversion(void);

#endif /* ADC_SAMPLER_H */
