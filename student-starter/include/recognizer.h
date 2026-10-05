#ifndef RECOGNIZER_H
#define RECOGNIZER_H

/* Pure recognition arithmetic: normalisation, distance, template averaging and
 * threshold calibration.
 *
 * Nothing in this module may touch an AVR register. Keep it that way: it is
 * what lets test/host_tests.cpp compile the same source on your laptop and
 * exercise it thousands of times per second, which is a much faster way to
 * find a scaling bug than staring at a Proteus terminal.
 *
 * Two helpers at the bottom - vpl_isqrt() and vpl_crc16() - are provided
 * complete. They are standard algorithms, not the point of the exercise.
 */

#include <stdint.h>
#include "config.h"

/** Raw per-utterance accumulators, before normalisation.
 *
 * Filled in by the feature extractor over the 1.6 s capture window. The units
 * are deliberately different per field; collapsing them onto one comparable
 * scale is what vpl_normalize() is for.
 */
struct RawFeatures {
    /** Sum of sample^2 over the 1600 samples of each segment. Max 1600*512^2. */
    uint32_t energy[SEGMENT_COUNT];
    /** Zero crossings counted over the 1600 samples of each segment. */
    uint16_t zcr[SEGMENT_COUNT];
    /** Sum of bin magnitudes over the 25 frames of each segment, per band. */
    uint32_t band[SEGMENT_COUNT][BAND_COUNT];
};

/** TODO(features): collapse RawFeatures onto FEATURE_COUNT unsigned 8-bit codes.
 *
 * Required layout of `out`, because the EEPROM record, the distance metric and
 * the grading tests all assume it: segment s occupies
 * out[s*10 .. s*10+9], with index FEATURE_IDX_ENERGY the energy, index
 * FEATURE_IDX_ZCR the zero-crossing rate and indices FEATURE_IDX_BAND0..+7 the
 * eight band levels.
 *
 * Requirements your implementation must meet:
 *   - Multiplying every raw accumulator by a constant (the same phrase, spoken
 *     louder) must not change the output. If it does, you have built a volume
 *     meter, not a lock.
 *   - An all-zero RawFeatures must produce all zeros, and must not divide by
 *     zero on the way there.
 *   - Nothing may overflow. `energy` alone reaches 4.2e8; think about what
 *     happens when you multiply that by 255 in a uint32_t.
 *
 * See the TODO(features) block in config.h for the design questions.
 */
void vpl_normalize(const RawFeatures *raw, uint8_t *out);

/** TODO(matching): weighted Manhattan distance between two feature vectors.
 *
 *   distance = sum over i of weight(i) * |a[i] - b[i]|
 *
 * Requirements:
 *   - vpl_distance(a, a) == 0 for every a.
 *   - vpl_distance(a, b) == vpl_distance(b, a).
 *   - The weight pattern repeats identically for all 8 segments.
 *   - No square root and no squaring: absolute differences are cheap on an AVR
 *     and produce the same ranking the threshold test needs.
 *
 * Work out the largest value this can return and make sure your return type
 * holds it - you will need that number for your threshold ceiling.
 */
uint32_t vpl_distance(const uint8_t *a, const uint8_t *b);

/** TODO(training): average `count` feature vectors into one template.
 *
 * Watch the accumulator type: three 0..255 values sum to 765, which does not
 * fit in the uint8_t you are averaging. Round to nearest rather than
 * truncating, or repeated retraining drifts the template downwards.
 */
void vpl_template_average(const uint8_t *const *vectors, uint8_t count, uint8_t *out);

/** TODO(threshold): derive an acceptance threshold from the training set.
 *
 * The threshold may not be a magic number. It has to be measured from how much
 * the three training utterances disagree with each other, so that a speaker
 * who repeats themselves consistently gets a tight lock and one who does not
 * gets a looser one.
 *
 * A workable shape, which you are free to improve on if you can justify it:
 *   1. measure the spread of the training vectors against `tmpl`;
 *   2. multiply it by a safety margin;
 *   3. add a floor, and explain in your report what happens without it when
 *      three recordings are nearly identical;
 *   4. clamp to a ceiling, and explain what happens without it when they are
 *      wildly different.
 *
 * Measure the spread in the same units the test-time comparison produces, or
 * the threshold will not mean what you think it means.
 */
uint32_t vpl_threshold(const uint8_t *const *vectors, uint8_t count, const uint8_t *tmpl);

/* --------------------------------------------------------------------------
 * Provided helpers - complete, no work needed.
 * ------------------------------------------------------------------------ */

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), for the EEPROM record. */
uint16_t vpl_crc16(const uint8_t *data, uint16_t len);

/** Integer square root, floor(sqrt(v)). No division, no floating point. */
uint32_t vpl_isqrt(uint32_t v);

#endif /* RECOGNIZER_H */
