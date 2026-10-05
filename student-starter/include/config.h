#ifndef CONFIG_H
#define CONFIG_H

/* Central tuning knobs for the voice-authenticated parking lock.
 *
 * The constants that define the system's architecture - sample rate, frame
 * size, capture length, timer values - are given to you, because the whole
 * class needs to be building the same machine for the results to be
 * comparable.
 *
 * The constants that define its *behaviour* - detection thresholds, feature
 * scaling, distance weights, the safety margin on the acceptance threshold -
 * are marked TODO. Those are design decisions, and defending your choice of
 * them is part of the assignment. Put every one of them here with a comment
 * saying what unit it is in and why that value; a bare number buried in a .cpp
 * file costs marks under "no magic numbers".
 */

#include <stdint.h>

/* ------------------------------------------------------------------ clock */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

/* -------------------------------------------------------------- sampling */

/** Audio sample rate, Hz. Timer2 CTC: 16 MHz / (8 * (249 + 1)) = 8000 Hz. */
#define SAMPLE_RATE_HZ 8000u
/** Timer2 prescaler used for both sampling and playback. */
#define TIMER2_PRESCALER 8u
/** Timer2 compare value: OCR2A = F_CPU / (prescaler * rate) - 1 = 249. */
#define TIMER2_OCR_VALUE 249u

/** Analysis frame length in samples. 64 samples @ 8 kHz = 8.0 ms. */
#define FRAME_LEN 64u
/** Frame period in milliseconds, for timing budgets and documentation. */
#define FRAME_MS 8u

/* --------------------------------------------------------------- capture */

/** Frames captured per utterance. 200 * 8 ms = 1600 ms = 1.6 s. */
#define CAPTURE_FRAMES 200u
/** Temporal segments the utterance is split into. */
#define SEGMENT_COUNT 8u
/** 200 / 8 = 25 frames per segment = 200 ms per segment. */
#define FRAMES_PER_SEGMENT (CAPTURE_FRAMES / SEGMENT_COUNT)

/* ------------------------------------------------------------------- FFT */

/** FFT size. Must equal FRAME_LEN. */
#define FFT_N 64u
/** log2(FFT_N). */
#define FFT_LOG2 6u
/** FFT bin width in Hz: 8000 / 64 = 125 Hz. */
#define FFT_BIN_HZ (SAMPLE_RATE_HZ / FFT_N)

/** Left shift applied to windowed samples before the FFT.
 *
 * The centred ADC value spans +-512, which only uses 10 of the 16 available
 * bits, and the radix-2 butterfly divides by 2 at each of its 6 stages. Without
 * a pre-shift the quiet bins would be quantised away. Shifting left by 4 gives
 * a +-8192 input and a worst-case bin magnitude of 4096, still inside int16_t.
 */
#define FFT_INPUT_SHIFT 4

/* --------------------------------------------------------------- features */

/** Spectral bands per segment. */
#define BAND_COUNT 8u
/** Energy + zero-crossing rate + BAND_COUNT band energies. */
#define FEATURES_PER_SEGMENT (2u + BAND_COUNT)
/** 8 segments * 10 features = 80. */
#define FEATURE_COUNT (SEGMENT_COUNT * FEATURES_PER_SEGMENT)

/** Index of the energy feature inside one segment's 10-feature block. */
#define FEATURE_IDX_ENERGY 0u
/** Index of the zero-crossing-rate feature inside one segment's block. */
#define FEATURE_IDX_ZCR 1u
/** Index of the first band-energy feature inside one segment's block. */
#define FEATURE_IDX_BAND0 2u

/* TODO(features): decide how a raw accumulator becomes a 0..255 feature code.
 *
 * The three raw quantities have wildly different units and ranges: energy is a
 * sum of squares up to about 4.2e8, the zero-crossing count is 0..1600, and a
 * band magnitude is a sum over 25 frames. They all have to end up on one 0..255
 * scale or the distance metric will be dominated by whichever happens to be
 * numerically largest.
 *
 * Questions to answer here, each with a named constant:
 *   - Which of the three should be scaled per utterance (so that a loud and a
 *     quiet recording of the same phrase look the same), and which should be
 *     on a fixed absolute scale? Getting this wrong makes the lock a volume
 *     meter - it will happily accept anyone who shouts.
 *   - Speech spectra have a very large dynamic range. Does a straight linear
 *     scale keep the quiet bands distinguishable, or do you need a compressive
 *     one (square root, log) first?
 *   - What is full scale for the zero-crossing count, and what happens above
 *     it?
 */

/* ---------------------------------------------------------------- matching */

/* TODO(matching): define the weights of the weighted Manhattan distance and the
 * threshold calibration constants.
 *
 *   distance = sum over i of weight(i) * |input[i] - template[i]|
 *
 * There are 8 band features per segment and only 2 temporal ones, so with equal
 * weights the spectrum outvotes the energy/ZCR contour 8 to 2. Decide what
 * split you want and justify it in your report: the spectrum mostly carries
 * *who* is speaking, the temporal contour mostly carries *what* they said, and
 * this system has to get both right.
 *
 * Then define how the acceptance threshold is derived from the three training
 * utterances (see recognizer.h). It must be computed, not a magic number, and
 * it needs both a floor and a ceiling - explain in your report what goes wrong
 * without each of them.
 */

/* --------------------------------------------------------------------- VAD */

/* TODO(vad): define the voice-activity-detection constants.
 *
 * At minimum you need: how many consecutive loud frames confirm the start of an
 * utterance, how many consecutive quiet frames confirm the end, how the speech
 * threshold is derived from the measured noise floor, and how fast the noise
 * floor estimate adapts. Give each one a unit in the comment - "frames" and
 * "sum of |sample| per frame" are the two you will need.
 */

/* ------------------------------------------------------------------- servo */

/** Timer1 TOP for 50 Hz: 16 MHz / 8 / 50 Hz = 40000 ticks, so TOP = 39999. */
#define SERVO_TOP 39999u
/** Timer1 prescaler for the servo PWM. One tick = 0.5 us. */
#define SERVO_PRESCALER 8u
/** Locked position pulse width in Timer1 ticks. 2000 * 0.5 us = 1.000 ms. */
#define SERVO_PULSE_LOCKED 2000u
/** Unlocked position pulse width in Timer1 ticks. 4000 * 0.5 us = 2.000 ms. */
#define SERVO_PULSE_UNLOCKED 4000u

/** How long the gate stays open after a successful match, in milliseconds. */
#define GATE_OPEN_MS 3000u

/* ------------------------------------------------------------------ button */

/** Debounce time for the training button, in milliseconds. */
#define BUTTON_DEBOUNCE_MS 25u
/** Hold time that counts as a long press (erase template), in milliseconds. */
#define BUTTON_LONG_PRESS_MS 2000u

/* --------------------------------------------------------------------- UI */

/** How long the reject LED stays lit, in milliseconds. */
#define REJECT_INDICATE_MS 1000u

/* ------------------------------------------------------------------- UART */

/** Debug UART baud rate. */
#define UART_BAUD 115200UL

/* ----------------------------------------------------------------- EEPROM */

/** Byte address of the stored template inside the 1 KB EEPROM. */
#define EEPROM_TEMPLATE_ADDR 0u
/** Identifies a template written by this firmware. */
#define TEMPLATE_MAGIC 0xA5C3u
/** Bumped whenever the feature layout changes, which invalidates old templates. */
#define TEMPLATE_VERSION 1u
/** Value an erased EEPROM cell reads back as. */
#define EEPROM_ERASED_BYTE 0xFFu

/* --------------------------------------------------------------- training */

/** Utterances collected before a template is built. */
#define TRAINING_UTTERANCES 3u

#endif /* CONFIG_H */
