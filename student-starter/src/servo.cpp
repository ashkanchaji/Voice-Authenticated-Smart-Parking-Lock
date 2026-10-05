#include "config.h"
#include "pins.h"
#include "servo.h"
#include <avr/io.h>

/* TODO(pwm): Timer1 hardware PWM for the gate servo on D9 / OC1A.
 *
 * Servo.h, Servo.write(), analogWrite() and delay() are all banned. Beyond the
 * assignment rule there are real reasons: analogWrite() on pin 9 gives 490 Hz
 * with 8-bit resolution, which is neither the frequency nor the precision a
 * hobby servo needs, and Servo.h would claim a timer this project has already
 * allocated.
 *
 * --- Target ---------------------------------------------------------------
 * Fast PWM with ICR1 as TOP (mode 14: WGM13:0 = 1110), prescaler 8,
 * ICR1 = SERVO_TOP:
 *     tick   = 8 / 16 MHz            = 0.5 us
 *     period = (39999 + 1) * 0.5 us  = 20.0 ms -> 50 Hz
 *     OCR1A  = pulse width in 0.5 us ticks
 * Set COM1A1 for a non-inverting output on OC1A, and remember that OC1A only
 * reaches the pin if PB1 is configured as an output.
 *
 * --- Why this makes the 3-second open time free ---------------------------
 * Once ICR1 and OCR1A are set the hardware generates the waveform on its own.
 * Holding the gate open costs zero CPU, which is what lets the main loop stay
 * non-blocking while the servo is out at 90 degrees and the audio is playing.
 *
 * --- Calibration ----------------------------------------------------------
 * 1.0 ms and 2.0 ms are the nominal endpoints, but a real servo and a real
 * gate arm will not agree with the nominal figures - the horn may hit its
 * mechanical stop early, or not reach 90 degrees at all. Keep
 * servo_configure() so the endpoints can be trimmed without a rebuild, and
 * clamp the values: a pulse outside roughly 0.5 ms to 2.5 ms will make a hobby
 * servo buzz against its end stop and draw stall current.
 */

void servo_pwm_init(void)
{
    /* TODO: set PB1 as an output, configure Timer1 mode 14 with ICR1 = SERVO_TOP
     * and prescaler 8, then drive the gate to the locked position. */
}

void servo_configure(uint16_t locked_ticks, uint16_t unlocked_ticks)
{
    (void)locked_ticks;
    (void)unlocked_ticks;
    /* TODO: store the clamped endpoints and re-apply the current one. */
}

void servo_lock(void)
{
    /* TODO: OCR1A = locked pulse width. */
}

void servo_unlock(void)
{
    /* TODO: OCR1A = unlocked pulse width. */
}

bool servo_is_unlocked(void)
{
    return false; /* TODO */
}

uint16_t servo_pulse_ticks(void)
{
    return 0; /* TODO: current OCR1A, so you can print it over UART. */
}
