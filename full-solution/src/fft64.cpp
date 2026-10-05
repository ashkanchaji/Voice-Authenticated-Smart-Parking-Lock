#include "fft64.h"
#include "dsp_tables.h"

/* ---------------------------------------------------------------------------
 * Adapted from:
 *   "FFTfix" - Bruce R. Land, ECE4760, Cornell University
 *   http://people.ece.cornell.edu/land/courses/ece4760/Math/GCC644/FFT/
 *       FFTfixGCC644_macro.c
 *   itself adapted from code by Tom Roberts (1989) and Malcolm Slaney (1994).
 *
 * Changes made for this project (ATmega328P, 2 KB SRAM):
 *   - N reduced from 128 to 64 points, so one FFT covers exactly one 8 ms
 *     analysis frame and the two working buffers cost 256 bytes instead of 512.
 *   - Twiddles moved from a runtime-computed SRAM array to a PROGMEM Q15 table
 *     (tools/gen_tables.py). The original built the table at boot with
 *     floating-point sin(), pulling in libm and spending 256 bytes of SRAM;
 *     here it costs 128 bytes of flash and no SRAM.
 *   - The Q8.8 `multfix` inline-assembler macro was replaced with a plain C
 *     Q15 multiply. The asm version is faster, but the C version is portable
 *     enough to compile for the host unit tests, and the measured cost is
 *     around 2.5 ms per frame against an 8 ms budget - there is no need to
 *     spend the readability.
 *   - The imaginary-part swap in the bit-reversal stage is restored. The
 *     Cornell version comments it out because its input is always real; this
 *     one does too, and the swap stays commented for the same reason, but the
 *     reason is now stated rather than implied.
 * ------------------------------------------------------------------------- */

/** Q15 multiply: (a * b) >> 15, with the product computed in 32 bits. */
static inline int16_t mul_q15(int16_t a, int16_t b)
{
    return (int16_t)(((int32_t)a * (int32_t)b) >> 15);
}

void fft64(int16_t *fr, int16_t *fi)
{
    const uint8_t n = (uint8_t)FFT_N;
    uint8_t mr = 0;
    uint8_t nn = n - 1;

    /* Decimation in time: reorder the input into bit-reversed order. Only the
     * real part is swapped because fft64_window() always zeroes fi. */
    for (uint8_t m = 1; m <= nn; ++m) {
        uint8_t l = n;
        do {
            l >>= 1;
        } while (mr + l > nn);
        mr = (uint8_t)((mr & (l - 1)) + l);
        if (mr <= m) {
            continue;
        }
        int16_t tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;
    }

    uint8_t l = 1;
    uint8_t k = (uint8_t)(FFT_LOG2 - 1u);
    while (l < n) {
        uint8_t istep = (uint8_t)(l << 1);
        for (uint8_t m = 0; m < l; ++m) {
            uint8_t j = (uint8_t)(m << k);
            /* cos(x) = sin(x + pi/2): the cosine twiddle is the sine table
             * read a quarter period ahead. */
            int16_t wr = (int16_t)pgm_read_word(&SINE_Q15[(j + FFT_N / 4u) & (FFT_N - 1u)]);
            int16_t wi = (int16_t)(-(int16_t)pgm_read_word(&SINE_Q15[j & (FFT_N - 1u)]));
            /* Halving the twiddles is one half of the per-stage 1/2 scaling;
             * the >>1 on q below is the other half. */
            wr >>= 1;
            wi >>= 1;

            for (uint8_t i = m; i < n; i = (uint8_t)(i + istep)) {
                uint8_t jj = (uint8_t)(i + l);
                int16_t tr = (int16_t)(mul_q15(wr, fr[jj]) - mul_q15(wi, fi[jj]));
                int16_t ti = (int16_t)(mul_q15(wr, fi[jj]) + mul_q15(wi, fr[jj]));
                int16_t qr = (int16_t)(fr[i] >> 1);
                int16_t qi = (int16_t)(fi[i] >> 1);
                fr[jj] = (int16_t)(qr - tr);
                fi[jj] = (int16_t)(qi - ti);
                fr[i] = (int16_t)(qr + tr);
                fi[i] = (int16_t)(qi + ti);
            }
        }
        --k;
        l = istep;
    }
}

void fft64_window(const int16_t *src, int16_t *fr, int16_t *fi)
{
    for (uint8_t i = 0; i < (uint8_t)FRAME_LEN; ++i) {
        int16_t w = (int16_t)pgm_read_word(&HAMMING_Q15[i]);
        /* One shift does both the Q15 normalisation and the FFT pre-scale:
         * (x * w) >> (15 - 4) == ((x * w) >> 15) << 4, without losing the
         * four low bits in between. */
        fr[i] = (int16_t)(((int32_t)src[i] * (int32_t)w) >> (15 - FFT_INPUT_SHIFT));
        fi[i] = 0;
    }
}

uint16_t fft64_magnitude(int16_t re, int16_t im)
{
    /* Promote before negating: on the AVR `int` is 16 bits, so -(-32768)
     * would overflow. The FFT cannot actually reach -32768 with the scaling
     * used here, but the cast makes that independent of the scaling. */
    uint16_t a = (uint16_t)(re < 0 ? -(int32_t)re : (int32_t)re);
    uint16_t b = (uint16_t)(im < 0 ? -(int32_t)im : (int32_t)im);
    uint16_t hi = a > b ? a : b;
    uint16_t lo = a > b ? b : a;
    return (uint16_t)(hi + (lo >> 2));
}
