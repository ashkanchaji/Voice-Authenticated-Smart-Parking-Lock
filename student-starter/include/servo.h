#ifndef SERVO_H
#define SERVO_H

/* Timer1 hardware PWM for the gate servo on D9 / OC1A.
 *
 * Fast PWM mode 14 (TOP = ICR1), prescaler 8:
 *     tick   = 8 / 16 MHz          = 0.5 us
 *     period = (39999 + 1) * 0.5us = 20.0 ms  -> 50 Hz
 *     OCR1A  = pulse width in 0.5 us ticks
 *
 * Servo.h and analogWrite() are not used. analogWrite() on pin 9 would give
 * 490 Hz with 8-bit resolution, which is neither the frequency nor the
 * precision a hobby servo needs, and Servo.h would take a timer this project
 * has already allocated.
 *
 * Once ICR1 and OCR1A are set the hardware generates the waveform on its own -
 * the gate holding its position costs zero CPU, which is what makes the 3 s
 * open time non-blocking.
 */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/** Configure Timer1 for 50 Hz PWM on OC1A and drive the gate to locked. */
void servo_pwm_init(void);

/** Move to the locked position (SERVO_PULSE_LOCKED). */
void servo_lock(void);

/** Move to the unlocked position (SERVO_PULSE_UNLOCKED). */
void servo_unlock(void);

/** True if the last command was servo_unlock(). */
bool servo_is_unlocked(void);

/** Override the two endpoint pulse widths, in 0.5 us Timer1 ticks.
 *
 * Real servos disagree about what 0 and 90 degrees mean; 1.0 ms / 2.0 ms is
 * the common nominal pair, but a given horn may need 0.9 ms / 2.1 ms to reach
 * the gate's mechanical stops. Values are clamped to a sane 0.5 ms .. 2.5 ms.
 */
void servo_configure(uint16_t locked_ticks, uint16_t unlocked_ticks);

/** Current pulse width in 0.5 us ticks, for UART tracing. */
uint16_t servo_pulse_ticks(void);

#endif /* SERVO_H */
