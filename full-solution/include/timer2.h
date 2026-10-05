#ifndef TIMER2_H
#define TIMER2_H

/* Timer2 is shared between two jobs that never run at the same time.
 *
 *   SAMPLING  - kick off an ADC conversion every 125 us (8 kHz)
 *   PLAYBACK  - push the next PCM byte to the R-2R ladder every 125 us
 *
 * Both need exactly the same 8 kHz tick, so the timer configuration is
 * identical and only the interrupt body differs. Because there can be only one
 * TIMER2_COMPA_vect in a program, this module owns it and dispatches on the
 * current mode; adc_sampler.cpp and dac_audio.cpp provide the two bodies.
 *
 * Timer ownership across the firmware:
 *   Timer0 - 1 ms system tick   (systick.cpp)
 *   Timer1 - 50 Hz servo PWM    (servo.cpp)
 *   Timer2 - 8 kHz audio clock  (this file)
 */

#include <stdint.h>

enum class Timer2Mode : uint8_t {
    OFF,      /**< interrupt disabled, timer stopped */
    SAMPLING, /**< ADC acquisition */
    PLAYBACK, /**< DAC output */
};

/** Set up CTC mode and OCR2A for 8 kHz. Leaves the timer in OFF. */
void timer2_init(void);

/** Switch jobs. Stops the clock in OFF, restarts it otherwise. */
void timer2_set_mode(Timer2Mode mode);

/** Current job. */
Timer2Mode timer2_mode(void);

/** Stop the timer from inside an interrupt handler.
 *
 * Same effect as timer2_set_mode(Timer2Mode::OFF), but written to be callable
 * from the ISR itself so a job can retire the clock the moment it finishes
 * instead of making the main loop poll for the end.
 */
void timer2_stop_isr(void);

#endif /* TIMER2_H */
