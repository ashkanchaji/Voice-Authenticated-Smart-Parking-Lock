#include "feature_extract.h"
#include "fft64.h"
#include "pgm_compat.h"
#include <string.h>

/* Band edges as inclusive FFT bin indices. Bin k is centred on k * 125 Hz.
 *
 *   band  bins    frequency range
 *   ----  ------  -----------------
 *     0    2..3     250 -  375 Hz
 *     1    4..5     500 -  625 Hz
 *     2    6..7     750 -  875 Hz
 *     3    8..10   1000 - 1250 Hz
 *     4   11..13   1375 - 1625 Hz
 *     5   14..17   1750 - 2125 Hz
 *     6   18..22   2250 - 2750 Hz
 *     7   23..28   2875 - 3500 Hz
 *
 * The bands widen with frequency, which is the same idea a mel filterbank
 * encodes: pitch resolution matters most at the low end. Bin 0 (DC) and bin 1
 * (125 Hz) are excluded because they carry the ADC bias residual and mains
 * hum, not speech; the top band stops at 3500 Hz, safely under the 4000 Hz
 * Nyquist limit and inside what the input anti-alias filter passes.
 */
static const uint8_t BAND_START[BAND_COUNT] PROGMEM = { 2, 4, 6, 8, 11, 14, 18, 23 };
static const uint8_t BAND_END[BAND_COUNT] PROGMEM   = { 3, 5, 7, 10, 13, 17, 22, 28 };

static RawFeatures s_raw;
static uint16_t s_frames;
static int8_t s_prev_sign;

/* FFT scratch. 256 bytes, and the single largest SRAM item in the firmware.
 * File-scope rather than on the stack so the budget in docs/memory-budget.md
 * is a static number instead of a guess about the deepest call path. */
static int16_t s_fr[FFT_N];
static int16_t s_fi[FFT_N];

uint8_t feature_band_start(uint8_t b)
{
    return pgm_read_byte(&BAND_START[b]);
}

uint8_t feature_band_end(uint8_t b)
{
    return pgm_read_byte(&BAND_END[b]);
}

uint16_t feature_frame_magnitude(const int16_t *frame)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < FRAME_LEN; ++i) {
        int16_t v = frame[i];
        sum = (uint16_t)(sum + (uint16_t)(v < 0 ? -(int32_t)v : (int32_t)v));
    }
    return sum;
}

void feature_reset(void)
{
    memset(&s_raw, 0, sizeof(s_raw));
    s_frames = 0;
    s_prev_sign = 0;
}

uint16_t feature_frames_done(void)
{
    return s_frames;
}

const RawFeatures *feature_raw(void)
{
    return &s_raw;
}

bool feature_process_frame(const int16_t *frame)
{
    if (s_frames >= CAPTURE_FRAMES) {
        return true;
    }
    const uint8_t seg = (uint8_t)(s_frames / FRAMES_PER_SEGMENT);

    /* --- time domain: short-time energy and zero-crossing count ---------- */
    uint32_t energy = 0;
    uint16_t crossings = 0;
    int8_t prev = s_prev_sign;
    for (uint8_t i = 0; i < FRAME_LEN; ++i) {
        int16_t v = frame[i];
        /* |v| <= 512, so v*v <= 262144 and 64 of them cannot overflow 32 bits.
         * Across a whole 1600-sample segment the worst case is 4.19e8, still
         * inside uint32_t's 4.29e9. */
        energy += (uint32_t)((int32_t)v * (int32_t)v);

        int8_t sign = (v > 0) ? 1 : (v < 0) ? -1 : prev;
        if (prev != 0 && sign != prev) {
            ++crossings;
        }
        prev = sign;
    }
    s_prev_sign = prev;
    s_raw.energy[seg] += energy;
    s_raw.zcr[seg] = (uint16_t)(s_raw.zcr[seg] + crossings);

    /* --- frequency domain: band magnitudes ------------------------------ */
    fft64_window(frame, s_fr, s_fi);
    fft64(s_fr, s_fi);
    for (uint8_t b = 0; b < BAND_COUNT; ++b) {
        const uint8_t first = pgm_read_byte(&BAND_START[b]);
        const uint8_t last = pgm_read_byte(&BAND_END[b]);
        uint32_t acc = 0;
        for (uint8_t k = first; k <= last; ++k) {
            acc += fft64_magnitude(s_fr[k], s_fi[k]);
        }
        s_raw.band[seg][b] += acc;
    }

    ++s_frames;
    return s_frames >= CAPTURE_FRAMES;
}

void feature_finalize(uint8_t *out)
{
    vpl_normalize(&s_raw, out);
}
