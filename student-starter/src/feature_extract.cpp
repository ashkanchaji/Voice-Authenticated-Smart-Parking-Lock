#include "feature_extract.h"
#include "fft64.h"
#include "pgm_compat.h"
#include <string.h>

/* TODO(features): streaming feature extraction.
 *
 * The utterance is 1.6 s = 12800 samples. That will never fit in 2 KB of SRAM,
 * so nothing may be stored at sample rate. Frames arrive one at a time, each is
 * folded into the accumulators for the segment it belongs to, and then it is
 * thrown away.
 *
 * Per 200 ms segment you owe ten numbers:
 *   [0] short-time energy      - sum of sample^2 over the segment
 *   [1] zero-crossing count    - sign changes over the segment
 *   [2..9] eight band levels   - summed FFT bin magnitudes
 *
 * --- The band table ------------------------------------------------------
 * Fill in BAND_START / BAND_END below. Bin k of the 64-point FFT is centred on
 * k * FFT_BIN_HZ = k * 125 Hz, and bins 0..31 are the usable half.
 *
 * Constraints, which you must justify in your report:
 *   - Cover roughly 250 Hz to 3500 Hz - the range that carries speech.
 *   - Do not include bin 0 (DC) or bin 1 (125 Hz): those hold the residual ADC
 *     bias and mains hum, not voice.
 *   - Do not go above bin 31; and think about where the input anti-alias
 *     filter's corner sits before you go near it.
 *   - The eight bands do not have to be the same width. Human pitch resolution
 *     is much finer at the low end than the high end, which is the idea a mel
 *     filterbank encodes. Widening the bands as frequency rises is closer to
 *     how hearing works than eight equal slices.
 * Write the resulting table into docs/recognition-algorithm.md.
 *
 * --- Getting it right ----------------------------------------------------
 *   - Zero crossings do not respect frame boundaries. Carry the sign of the
 *     last sample of one frame into the next, or you will lose one crossing
 *     per frame boundary - 25 per segment, which is not noise.
 *   - Check your accumulator widths. One segment's energy reaches 1600 * 512^2
 *     = 4.19e8. Which of uint16_t, uint32_t and uint64_t do you actually need,
 *     and what does each cost on an 8-bit core?
 *   - fft64_window() and fft64() are provided and complete. Call them; do not
 *     rewrite them.
 */

static const uint8_t BAND_START[BAND_COUNT] PROGMEM = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* TODO */
static const uint8_t BAND_END[BAND_COUNT] PROGMEM   = { 0, 0, 0, 0, 0, 0, 0, 0 }; /* TODO */

static RawFeatures s_raw;
static uint16_t s_frames;

/* Scratch for the FFT. File-scope rather than on the stack so that the number
 * in your memory budget is a fact instead of a guess about call depth. */
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
    (void)frame;
    return 0; /* TODO: sum of |sample| over the frame, for the VAD. */
}

void feature_reset(void)
{
    memset(&s_raw, 0, sizeof(s_raw));
    s_frames = 0;
    /* TODO: reset any cross-frame state you keep (see the zero-crossing note). */
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
    (void)frame;
    (void)s_fr;
    (void)s_fi;
    /* TODO: fold this frame into the accumulators for segment
     * (s_frames / FRAMES_PER_SEGMENT), then advance s_frames and report
     * whether the capture window is full. */
    return true;
}

void feature_finalize(uint8_t *out)
{
    vpl_normalize(&s_raw, out);
}
