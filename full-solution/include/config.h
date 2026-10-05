#ifndef CONFIG_H
#define CONFIG_H

/* Central tuning knobs for the voice-authenticated parking lock.
 *
 * Everything a grader or a student might reasonably want to change lives here,
 * with the unit spelled out in the name or the comment. Nothing in the .cpp
 * files should contain a bare numeric constant that means something.
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
 * bits. The radix-2 butterfly below divides by 2 at every one of the 6 stages,
 * so without a pre-shift the quiet bins would be quantised away. Shifting left
 * by 4 gives a +-8192 input; the worst-case bin magnitude is then 8192/2 =
 * 4096, still comfortably inside int16_t, and the per-stage scaling guarantees
 * no intermediate can exceed the input maximum.
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

/** Zero crossings per segment that map to feature code 255.
 *
 * A segment is 1600 samples. 800 crossings in 1600 samples is a 2 kHz
 * dominant frequency, comfortably above any voiced speech and around the top
 * of the fricative range, so it is a sensible full-scale point. Unlike energy
 * and the band levels, ZCR is already loudness-invariant, so it uses this
 * fixed scale instead of a per-utterance maximum - the absolute value carries
 * real phonetic information that a per-utterance rescale would destroy.
 */
#define ZCR_FULL_SCALE 800u

/* ---------------------------------------------------------------- matching */

/** Weight of the energy feature in the weighted Manhattan distance. */
#define WEIGHT_ENERGY 3u
/** Weight of the zero-crossing-rate feature. */
#define WEIGHT_ZCR 2u
/** Weight of each of the 8 band-energy features. */
#define WEIGHT_BAND 1u

/** Threshold = max training-to-template distance * NUM / DEN + FLOOR.
 *
 * NUM/DEN = 2/1 doubles the observed spread of the three training utterances,
 * which is the cheapest defensible way to turn "how much does this speaker
 * vary between repetitions" into "how much variation do we accept".
 */
#define THRESHOLD_MARGIN_NUM 2u
#define THRESHOLD_MARGIN_DEN 1u

/** Absolute floor added to the threshold.
 *
 * Three recordings made back to back can be almost identical, which would give
 * a near-zero threshold that rejects the authorised speaker on the very next
 * try. The floor is 4% of the theoretical maximum distance (26520), i.e. an
 * average deviation of ~10 codes out of 255 per feature.
 */
#define THRESHOLD_FLOOR 1060u

/** Hard ceiling on the stored threshold.
 *
 * 30% of the maximum possible distance. If the three training utterances
 * disagree by more than this the recording session was bad, and accepting a
 * threshold that loose would make the lock meaningless. The firmware clamps
 * and reports it over UART so the operator can retrain.
 */
#define THRESHOLD_MAX 7956u

/** Largest distance the metric can produce: 8 * (3 + 2 + 8*1) * 255 = 26520. */
#define DISTANCE_MAX \
    (SEGMENT_COUNT * (WEIGHT_ENERGY + WEIGHT_ZCR + BAND_COUNT * WEIGHT_BAND) * 255u)

/* --------------------------------------------------------------------- VAD */

/** Frames above threshold needed to declare SPEECH_START. 3 * 8 ms = 24 ms. */
#define VAD_START_FRAMES 3u
/** Frames below threshold needed to declare SPEECH_END. 25 * 8 ms = 200 ms. */
#define VAD_END_FRAMES 25u
/** Speech threshold = noise_floor * this multiplier + VAD_ABS_MARGIN. */
#define VAD_NOISE_MULT 3u
/** Absolute margin above the noise floor, in sum-of-|sample| units per frame.
 *
 * 64 samples * 8 codes of average deviation. Below this the "speech" is within
 * a couple of ADC LSBs of the noise floor and is not worth waking up for.
 */
#define VAD_ABS_MARGIN 512u
/** Noise-floor IIR shift: noise += (frame_mag - noise) >> this. 2^5 frames = 256 ms. */
#define VAD_NOISE_SHIFT 5u
/** Initial noise-floor estimate, before any frame has been seen. */
#define VAD_NOISE_INIT 128u

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
