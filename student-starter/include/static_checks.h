#ifndef STATIC_CHECKS_H
#define STATIC_CHECKS_H

/* Compile-time consistency checks.
 *
 * Each of these guards a constant in config.h whose breakage would otherwise
 * show up as a silent behavioural bug rather than a build failure: a sampling
 * rate that is 7% off, a segment count that does not divide the capture window,
 * a servo pulse longer than the PWM period.
 *
 * TODO(quality): add checks for the constants you define yourself. Anything
 * with a relationship another constant depends on belongs here - for example,
 * that your acceptance-threshold floor sits below your ceiling, that the
 * ceiling is below the largest distance your metric can produce, and that your
 * end-of-speech hangover is shorter than the capture window.
 */

#include <stdint.h>
#include "config.h"
#include "template_store.h"

static_assert(FRAME_LEN == FFT_N,
              "one analysis frame must be exactly one FFT");
static_assert((FFT_N & (FFT_N - 1u)) == 0u,
              "radix-2 FFT needs a power-of-two size");
static_assert((1u << FFT_LOG2) == FFT_N,
              "FFT_LOG2 must be log2(FFT_N)");
static_assert(CAPTURE_FRAMES % SEGMENT_COUNT == 0u,
              "segments must divide the capture window evenly");
static_assert((uint32_t)FRAMES_PER_SEGMENT * FRAME_LEN * 1000ul / SAMPLE_RATE_HZ == 200ul,
              "a segment must be 200 ms");
static_assert((uint32_t)CAPTURE_FRAMES * FRAME_LEN * 1000ul / SAMPLE_RATE_HZ == 1600ul,
              "the capture window must be 1600 ms");
static_assert(F_CPU / ((uint32_t)TIMER2_PRESCALER * (TIMER2_OCR_VALUE + 1ul)) == SAMPLE_RATE_HZ,
              "Timer2 prescaler and OCR2A do not produce SAMPLE_RATE_HZ");
static_assert(TIMER2_OCR_VALUE <= 255u,
              "OCR2A is an 8-bit register");
static_assert(F_CPU / ((uint32_t)SERVO_PRESCALER * (SERVO_TOP + 1ul)) == 50ul,
              "Timer1 TOP and prescaler do not produce 50 Hz");
static_assert(SERVO_PULSE_UNLOCKED < SERVO_TOP && SERVO_PULSE_LOCKED < SERVO_TOP,
              "servo pulse width must be shorter than the PWM period");
static_assert(FEATURE_COUNT == 80u,
              "the EEPROM record and the docs both assume 80 features");
static_assert(TRAINING_UTTERANCES >= 2u,
              "a template needs at least two utterances to have any spread");
static_assert(sizeof(StoredTemplate) <= 1024u,
              "the record must fit in the ATmega328P's 1 KB EEPROM");

#endif /* STATIC_CHECKS_H */
