#ifndef STATIC_CHECKS_H
#define STATIC_CHECKS_H

/* Compile-time consistency checks.
 *
 * Every one of these guards a constant in config.h that is easy to change and
 * whose breakage would otherwise show up as a silent behavioural bug: a
 * sampling rate that is 7% off, a segment count that does not divide the
 * capture, a threshold that cannot be represented. Failing the build is much
 * cheaper than debugging that in Proteus.
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
static_assert(THRESHOLD_FLOOR < THRESHOLD_MAX,
              "threshold floor must sit below the ceiling");
static_assert(THRESHOLD_MAX < DISTANCE_MAX,
              "a threshold at or above the maximum distance accepts everything");
static_assert(TRAINING_UTTERANCES >= 2u,
              "a template needs at least two utterances to have any spread");
static_assert(sizeof(StoredTemplate) <= 1024u,
              "the record must fit in the ATmega328P's 1 KB EEPROM");
static_assert(VAD_END_FRAMES < CAPTURE_FRAMES,
              "the end-of-speech hangover must be shorter than the capture");

#endif /* STATIC_CHECKS_H */
