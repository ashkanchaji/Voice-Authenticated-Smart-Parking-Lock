/* Cycle-count benchmark for the per-frame DSP work, run under simavr.
 *
 * The whole real-time argument for this design is that one 64-sample frame can
 * be windowed, transformed and reduced to features in well under the 8 ms that
 * the next frame takes to arrive. That claim deserves a measurement rather than
 * an estimate, so this firmware runs the real feature_process_frame() with
 * Timer1 free-running at the CPU clock (prescaler 1, so one tick is one cycle)
 * and prints the count.
 *
 * Build and run:  ./test/bench.sh
 *
 * Output goes through simavr's console register rather than the UART, because
 * a UART at 115200 baud would itself take longer than the thing being measured.
 */

#include "config.h"
#include "feature_extract.h"
#include "fft64.h"

#include <avr/io.h>
#include <math.h>

/* The .mmcu descriptor that wires these two registers up lives in
 * bench_mmcu.c. Writing a byte to GPIOR0 puts it on simavr's stdout; writing
 * SIMAVR_EXIT_OK to GPIOR1 ends the simulation. */
static const uint8_t SIMAVR_EXIT_OK = 4; /* SIMAVR_CMD_EXIT_CODE_0 */

static void put(char c) { GPIOR0 = (uint8_t)c; }

static void put_str(const char *s)
{
    while (*s) {
        put(*s++);
    }
}

static void put_u32(uint32_t v)
{
    char buf[11];
    uint8_t n = 0;
    do {
        buf[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v);
    while (n) {
        put(buf[--n]);
    }
}

/** Report a per-iteration cycle count and what it means at 16 MHz and 8 kHz. */
static void report(const char *label, uint32_t cycles, uint16_t iterations)
{
    const uint32_t per = cycles / iterations;
    put_str(label);
    put_str(": ");
    put_u32(per);
    put_str(" cycles, ");
    put_u32(per / 16u);          /* 16 cycles = 1 us at 16 MHz */
    put_str(".");
    put_u32(((per % 16u) * 10u) / 16u);
    put_str(" us, ");
    put_u32(per * 100u / (F_CPU / SAMPLE_RATE_HZ * FRAME_LEN));
    put_str("% of the 8 ms frame budget\r\n");
}

int main(void)
{
    /* Timer1 free-running, prescaler 1: TCNT1 counts CPU cycles directly and
     * wraps after 65536 of them, i.e. 4.1 ms. */
    TCCR1A = 0;
    TCCR1B = _BV(CS10);

    static int16_t frame[FRAME_LEN];
    for (uint8_t i = 0; i < FRAME_LEN; ++i) {
        frame[i] = (int16_t)(400.0 * sin(2.0 * M_PI * 7.0 * i / FRAME_LEN));
    }

    static int16_t fr[FFT_N];
    static int16_t fi[FFT_N];

    TCNT1 = 0;
    fft64_window(frame, fr, fi);
    fft64(fr, fi);
    report("window + fft64      ", TCNT1, 1);

    feature_reset();
    TCNT1 = 0;
    (void)feature_process_frame(frame);
    report("feature_process_frame", TCNT1, 1);

    /* Average over a whole 200 ms segment. Timer1 wraps after 65536 cycles,
     * which is less than three frames' worth, so the total has to be
     * accumulated one frame at a time in 32 bits rather than read out once at
     * the end. */
    feature_reset();
    uint32_t total = 0;
    for (uint8_t i = 0; i < 25; ++i) {
        TCNT1 = 0;
        (void)feature_process_frame(frame);
        total += TCNT1;
    }
    report("             (x25 avg)", total, 25);

    put_str("frame budget        : ");
    put_u32((uint32_t)F_CPU / SAMPLE_RATE_HZ * FRAME_LEN);
    put_str(" cycles (8.0 ms)\r\n");

    GPIOR1 = SIMAVR_EXIT_OK;
    for (;;) {
    }
}
