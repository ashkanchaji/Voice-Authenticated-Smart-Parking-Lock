#ifndef FFT64_H
#define FFT64_H

/* 64-point fixed-point radix-2 FFT.
 *
 * Adapted from Bruce Land's ECE4760 fixed-point FFT (Cornell University),
 * which is itself derived from Tom Roberts' 1989 fix_fft as revised by
 * Malcolm Slaney in 1994. See THIRD_PARTY_NOTICES.md for the provenance and
 * docs/recognition-algorithm.md for what was changed and why.
 */

#include <stdint.h>
#include "config.h"

/** Forward FFT, in place, on FFT_N points.
 *
 * `fr` and `fi` hold the real and imaginary parts. Every butterfly stage
 * scales its output by 1/2, so the result is the true DFT divided by FFT_N and
 * no intermediate value can exceed the largest input magnitude. That is what
 * makes the transform overflow-proof in int16_t without any block-floating-
 * point bookkeeping.
 */
void fft64(int16_t *fr, int16_t *fi);

/** Apply the Hamming window and the FFT_INPUT_SHIFT pre-scale.
 *
 * `src` holds FRAME_LEN centred ADC samples (nominally -512..+511). `fr` gets
 * the windowed, scaled result and `fi` is zeroed.
 */
void fft64_window(const int16_t *src, int16_t *fr, int16_t *fi);

/** Magnitude of one bin, approximated as max + min/4.
 *
 * The exact sqrt(re^2 + im^2) would cost a 32-bit square root per bin; this
 * approximation is worth up to +11.8% at 45 degrees and nothing at 0 or 90.
 * Because the same approximation is applied when the template is trained and
 * when a test utterance is scored, the bias cancels in the distance metric.
 */
uint16_t fft64_magnitude(int16_t re, int16_t im);

#endif /* FFT64_H */
