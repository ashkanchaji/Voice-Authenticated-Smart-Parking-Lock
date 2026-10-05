#ifndef RECOGNIZER_H
#define RECOGNIZER_H

/* Pure recognition arithmetic: normalisation, distance, template averaging and
 * threshold calibration.
 *
 * Nothing in this module touches an AVR register, so the whole file also
 * compiles for the host and is covered by test/test_recognizer.cpp.
 */

#include <stdint.h>
#include "config.h"

/** Raw per-utterance accumulators, before normalisation.
 *
 * Filled in by the feature extractor over the 1.6 s capture window. The units
 * are deliberately different per field and are documented on each one; the
 * whole point of vpl_normalize() is to collapse them onto one 0..255 scale.
 */
struct RawFeatures {
    /** Sum of sample^2 over the 1600 samples of each segment. Max 1600*512^2. */
    uint32_t energy[SEGMENT_COUNT];
    /** Zero crossings counted over the 1600 samples of each segment. */
    uint16_t zcr[SEGMENT_COUNT];
    /** Sum of bin magnitudes over the 25 frames of each segment, per band. */
    uint32_t band[SEGMENT_COUNT][BAND_COUNT];
};

/** Collapse RawFeatures onto FEATURE_COUNT unsigned 8-bit codes.
 *
 * Layout of `out`: segment s occupies out[s*10 .. s*10+9], with index 0 the
 * energy, index 1 the ZCR and indices 2..9 the eight band levels.
 *
 * Scaling, and why:
 *   energy -> 255 * sqrt(E[s] / Emax), Emax = max over segments.
 *       Per-utterance, so a quiet recording and a loud one of the same phrase
 *       give the same contour. The square root moves the value from the power
 *       domain to the amplitude domain, which spreads the quiet segments out
 *       instead of crushing them all to 0.
 *   zcr -> 255 * min(1, Z[s] / ZCR_FULL_SCALE), a fixed scale (see config.h).
 *   band -> 255 * B[s][b] / Bmax, Bmax = max over all segments and bands.
 *       One global maximum rather than a per-segment one, so a silent segment
 *       stays near zero instead of having its noise amplified to full scale.
 *       These are already magnitudes, not powers, so no square root is needed.
 *
 * An all-silent utterance (every accumulator zero) yields all zeros rather
 * than a divide by zero.
 */
void vpl_normalize(const RawFeatures *raw, uint8_t *out);

/** Weighted Manhattan distance between two normalised feature vectors.
 *
 * distance = sum over i of weight(i) * |a[i] - b[i]|, with the weights from
 * config.h: 3 for energy, 2 for ZCR, 1 for each band. Without the weighting
 * the eight band features would outvote the two temporal ones 8:2 and the
 * phrase contour would barely matter; 3:2:8 puts 23% of the vote on the energy
 * contour, 15% on ZCR and 62% on the spectrum.
 *
 * No square root, no squaring: absolute differences are single-cycle on the
 * AVR and the ranking they produce is what the threshold test needs.
 * Maximum possible value is DISTANCE_MAX (26520).
 */
uint32_t vpl_distance(const uint8_t *a, const uint8_t *b);

/** Average TRAINING_UTTERANCES feature vectors into one template.
 *
 * The sum of three 0..255 values reaches 765, so it is accumulated in 16 bits
 * before the divide. Rounding is to nearest (+1 before dividing by 3) so that
 * repeated retraining does not drift downward.
 */
void vpl_template_average(const uint8_t *const *vectors, uint8_t count, uint8_t *out);

/** Derive an acceptance threshold from how much the training set disagrees.
 *
 *   spread     = max over i of vpl_distance(vectors[i], tmpl)
 *   threshold  = spread * THRESHOLD_MARGIN_NUM / THRESHOLD_MARGIN_DEN
 *                       + THRESHOLD_FLOOR
 *   threshold  = min(threshold, THRESHOLD_MAX)
 *
 * `spread` is measured against the template rather than pairwise between the
 * utterances because that is exactly the quantity the test-time comparison
 * produces, so the threshold is in the same units as the thing it gates.
 * The floor keeps three near-identical recordings from producing a threshold
 * so tight that the speaker cannot pass; the ceiling keeps three bad ones from
 * producing a threshold so loose that everybody passes.
 */
uint32_t vpl_threshold(const uint8_t *const *vectors, uint8_t count, const uint8_t *tmpl);

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF), used for the EEPROM record.
 *
 * Bitwise rather than table-driven: 90 bytes is 720 iterations, about 50 us,
 * and a 512-byte lookup table would be a poor trade for that.
 */
uint16_t vpl_crc16(const uint8_t *data, uint16_t len);

/** Integer square root, floor(sqrt(v)). Used by the energy normalisation. */
uint32_t vpl_isqrt(uint32_t v);

#endif /* RECOGNIZER_H */
