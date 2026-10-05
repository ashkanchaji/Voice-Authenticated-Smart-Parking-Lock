#include "config.h"
#include "pins.h"
#include "servo.h"
#include <avr/io.h>

/** 0.5 ms and 2.5 ms in Timer1 ticks: outside this a hobby servo stalls. */
static const uint16_t SERVO_TICKS_MIN = 1000u;
static const uint16_t SERVO_TICKS_MAX = 5000u;

static uint16_t s_locked = SERVO_PULSE_LOCKED;
static uint16_t s_unlocked = SERVO_PULSE_UNLOCKED;
static bool s_unlocked_now;

static uint16_t clamp_ticks(uint16_t t)
{
    if (t < SERVO_TICKS_MIN) {
        return SERVO_TICKS_MIN;
    }
    return t > SERVO_TICKS_MAX ? SERVO_TICKS_MAX : t;
}

void servo_pwm_init(void)
{
    SERVO_DDR |= _BV(SERVO_BIT); /* PB1 / OC1A as output */

    /* Mode 14: WGM13:0 = 1110, TOP = ICR1, non-inverting output on OC1A. */
    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11); /* prescaler 8 */
    ICR1 = SERVO_TOP;
    TCNT1 = 0;
    servo_lock();
}

void servo_configure(uint16_t locked_ticks, uint16_t unlocked_ticks)
{
    s_locked = clamp_ticks(locked_ticks);
    s_unlocked = clamp_ticks(unlocked_ticks);
    /* Re-apply so a reconfiguration takes effect without a state change. */
    OCR1A = s_unlocked_now ? s_unlocked : s_locked;
}

void servo_lock(void)
{
    s_unlocked_now = false;
    OCR1A = s_locked;
}

void servo_unlock(void)
{
    s_unlocked_now = true;
    OCR1A = s_unlocked;
}

bool servo_is_unlocked(void)
{
    return s_unlocked_now;
}

uint16_t servo_pulse_ticks(void)
{
    return OCR1A;
}
