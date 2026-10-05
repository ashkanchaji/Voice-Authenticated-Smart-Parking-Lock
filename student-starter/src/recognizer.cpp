#include "recognizer.h"
#include "pgm_compat.h"
#include <string.h>

/* See recognizer.h for what each function must do and the properties it has to
 * satisfy. test/host_tests.cpp checks those properties on your laptop - run it
 * before you go anywhere near Proteus. */

void vpl_normalize(const RawFeatures *raw, uint8_t *out)
{
    (void)raw;
    /* TODO(features): scale the accumulators onto 0..255 per feature. */
    memset(out, 0, FEATURE_COUNT);
}

uint32_t vpl_distance(const uint8_t *a, const uint8_t *b)
{
    (void)a;
    (void)b;
    return 0; /* TODO(matching): weighted Manhattan distance. */
}

void vpl_template_average(const uint8_t *const *vectors, uint8_t count, uint8_t *out)
{
    (void)vectors;
    (void)count;
    /* TODO(training): average the training vectors into a template. */
    memset(out, 0, FEATURE_COUNT);
}

uint32_t vpl_threshold(const uint8_t *const *vectors, uint8_t count, const uint8_t *tmpl)
{
    (void)vectors;
    (void)count;
    (void)tmpl;
    /* TODO(threshold): derive the acceptance threshold from the training set.
     *
     * Returning 0 means "accept nothing", which is the right way for a lock to
     * fail while it is unimplemented. Do not start from a large constant. */
    return 0;
}

/* --------------------------------------------------------------------------
 * Provided helpers below this line - complete, no work needed.
 * ------------------------------------------------------------------------ */

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
