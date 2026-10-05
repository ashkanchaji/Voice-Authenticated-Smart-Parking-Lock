#include "recognizer.h"
#include "pgm_compat.h"

/** Per-feature weights for one segment, indexed the same way as the block. */
static const uint8_t FEATURE_WEIGHTS[FEATURES_PER_SEGMENT] PROGMEM = {
    WEIGHT_ENERGY, WEIGHT_ZCR,
    WEIGHT_BAND, WEIGHT_BAND, WEIGHT_BAND, WEIGHT_BAND,
    WEIGHT_BAND, WEIGHT_BAND, WEIGHT_BAND, WEIGHT_BAND,
};

uint32_t vpl_isqrt(uint32_t v)
{
    /* Bit-by-bit restoring square root: one iteration per pair of bits, no
     * division and no floating point. 16 iterations worst case. */
    uint32_t rem = 0;
    uint32_t root = 0;
    for (uint8_t i = 0; i < 16; ++i) {
        root <<= 1;
        rem = (rem << 2) | (v >> 30);
        v <<= 2;
        if (root < rem) {
            rem -= root | 1u;
            root |= 2u;
        }
    }
    return root >> 1;
}

/** Scale `v` into 0..`full` given that `max` maps to `full`, without overflow.
 *
 * Both operands are halved until `max * full` is guaranteed to fit in 32 bits.
 * Halving loses at most one bit of the ratio, which is far below the 1/255
 * quantisation step the result is heading for anyway.
 */
static uint32_t scale_to(uint32_t v, uint32_t max, uint32_t full)
{
    if (max == 0u) {
        return 0u;
    }
    const uint32_t limit = 0xFFFFFFFFul / full;
    while (max > limit) {
        max >>= 1;
        v >>= 1;
    }
    uint32_t r = (v * full) / max;
    return r > full ? full : r;
}

void vpl_normalize(const RawFeatures *raw, uint8_t *out)
{
    uint32_t e_max = 0;
    uint32_t b_max = 0;
    for (uint8_t s = 0; s < SEGMENT_COUNT; ++s) {
        if (raw->energy[s] > e_max) {
            e_max = raw->energy[s];
        }
        for (uint8_t b = 0; b < BAND_COUNT; ++b) {
            if (raw->band[s][b] > b_max) {
                b_max = raw->band[s][b];
            }
        }
    }

    for (uint8_t s = 0; s < SEGMENT_COUNT; ++s) {
        uint8_t *seg = &out[(uint16_t)s * FEATURES_PER_SEGMENT];

        /* 255 * sqrt(E/Emax), computed as sqrt(255^2 * E / Emax). */
        seg[FEATURE_IDX_ENERGY] = (uint8_t)vpl_isqrt(scale_to(raw->energy[s], e_max, 65025u));

        uint32_t z = ((uint32_t)raw->zcr[s] * 255u) / ZCR_FULL_SCALE;
        seg[FEATURE_IDX_ZCR] = (uint8_t)(z > 255u ? 255u : z);

        for (uint8_t b = 0; b < BAND_COUNT; ++b) {
            seg[FEATURE_IDX_BAND0 + b] = (uint8_t)scale_to(raw->band[s][b], b_max, 255u);
        }
    }
}

uint32_t vpl_distance(const uint8_t *a, const uint8_t *b)
{
    uint32_t sum = 0;
    uint8_t i = 0;
    /* Walk segment by segment so the weight index is a counter rather than a
     * modulo - `i % 10` would be 80 divisions on a chip with no divider. */
    for (uint8_t s = 0; s < SEGMENT_COUNT; ++s) {
        for (uint8_t f = 0; f < FEATURES_PER_SEGMENT; ++f, ++i) {
            uint8_t w = pgm_read_byte(&FEATURE_WEIGHTS[f]);
            int16_t d = (int16_t)a[i] - (int16_t)b[i];
            sum += (uint32_t)w * (uint32_t)(d < 0 ? -d : d);
        }
    }
    return sum;
}

void vpl_template_average(const uint8_t *const *vectors, uint8_t count, uint8_t *out)
{
    if (count == 0u) {
        return;
    }
    for (uint8_t i = 0; i < FEATURE_COUNT; ++i) {
        uint16_t sum = 0; /* count <= 3 and each term <= 255, so max 765. */
        for (uint8_t v = 0; v < count; ++v) {
            sum = (uint16_t)(sum + vectors[v][i]);
        }
        out[i] = (uint8_t)((sum + count / 2u) / count); /* round to nearest */
    }
}

uint32_t vpl_threshold(const uint8_t *const *vectors, uint8_t count, const uint8_t *tmpl)
{
    uint32_t spread = 0;
    for (uint8_t v = 0; v < count; ++v) {
        uint32_t d = vpl_distance(vectors[v], tmpl);
        if (d > spread) {
            spread = d;
        }
    }
    uint32_t t = spread * THRESHOLD_MARGIN_NUM / THRESHOLD_MARGIN_DEN + THRESHOLD_FLOOR;
    return t > THRESHOLD_MAX ? (uint32_t)THRESHOLD_MAX : t;
}

uint16_t vpl_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}
