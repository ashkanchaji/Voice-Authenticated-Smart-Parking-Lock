/* Host-side checks for the parts of the firmware that are pure arithmetic.
 *
 * Everything under test here compiles for the AVR and for the host from the
 * same sources - no mocks, no #ifdef branches in the production code. The only
 * accommodation is pgm_compat.h, which turns the flash-access macros into
 * plain dereferences off-target.
 *
 * Build and run:  ./test/run.sh
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "recognizer.h"
#include "fft64.h"
#include "feature_extract.h"
#include "vad.h"

static int g_checks;

#define CHECK(cond)                                                     \
    do {                                                                \
        ++g_checks;                                                     \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            return 1;                                                   \
        }                                                               \
    } while (0)

/* Deterministic LCG, so a failure is always reproducible. */
static uint32_t g_seed = 1u;
static void seed(uint32_t s) { g_seed = s; }
static uint32_t rnd(void)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return (g_seed >> 16) & 0x7FFFu;
}
/** Uniform integer in [-span, +span]. */
static int jitter(int span) { return (int)(rnd() % (uint32_t)(2 * span + 1)) - span; }

/* ------------------------------------------------------------- isqrt/crc */

static int test_isqrt(void)
{
    CHECK(vpl_isqrt(0) == 0);
    CHECK(vpl_isqrt(1) == 1);
    CHECK(vpl_isqrt(2) == 1);
    CHECK(vpl_isqrt(65025) == 255); /* the exact value the energy scale needs */
    CHECK(vpl_isqrt(65024) == 254);
    CHECK(vpl_isqrt(0xFFFFFFFFul) == 65535);
    for (uint32_t v = 0; v < 40000u; v += 7u) {
        uint32_t r = vpl_isqrt(v);
        CHECK(r * r <= v);
        CHECK((r + 1u) * (r + 1u) > v);
    }
    return 0;
}

static int test_crc(void)
{
    /* The published check value for CRC-16/CCITT-FALSE. */
    const char *s = "123456789";
    CHECK(vpl_crc16((const uint8_t *)s, 9) == 0x29B1u);
    /* A single flipped bit must change the CRC, or the EEPROM guard is useless. */
    uint8_t buf[16];
    for (int i = 0; i < 16; ++i) buf[i] = (uint8_t)(i * 17);
    uint16_t a = vpl_crc16(buf, 16);
    buf[7] ^= 0x01u;
    CHECK(vpl_crc16(buf, 16) != a);
    return 0;
}

/* ------------------------------------------------------------ normalize */

static void fill_raw(RawFeatures *r, uint32_t scale)
{
    std::memset(r, 0, sizeof(*r));
    for (unsigned s = 0; s < SEGMENT_COUNT; ++s) {
        r->energy[s] = (uint32_t)(s + 1u) * 1000u * scale;
        r->zcr[s] = (uint16_t)(s * 100u);
        for (unsigned b = 0; b < BAND_COUNT; ++b) {
            r->band[s][b] = (uint32_t)((s + 1u) * (b + 1u)) * 100u * scale;
        }
    }
}

static int test_normalize(void)
{
    RawFeatures r;
    uint8_t a[FEATURE_COUNT], b[FEATURE_COUNT];

    /* Loudness invariance: multiplying every raw accumulator by 64 must not
     * move the normalised vector. This is the property that stops the lock
     * from being a volume meter. */
    fill_raw(&r, 1);
    vpl_normalize(&r, a);
    fill_raw(&r, 64);
    vpl_normalize(&r, b);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) {
        /* ZCR is on a fixed scale so it is bit-exact; the others may differ by
         * one code because scale_to() halves its operands to avoid overflow. */
        CHECK(std::abs((int)a[i] - (int)b[i]) <= 1);
    }

    /* The loudest segment must map to full scale on both energy and bands. */
    CHECK(a[(SEGMENT_COUNT - 1u) * FEATURES_PER_SEGMENT + FEATURE_IDX_ENERGY] == 255);
    CHECK(a[(SEGMENT_COUNT - 1u) * FEATURES_PER_SEGMENT + FEATURE_IDX_BAND0 + BAND_COUNT - 1u] == 255);

    /* Silence must not divide by zero, and must not invent features. */
    std::memset(&r, 0, sizeof(r));
    vpl_normalize(&r, a);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(a[i] == 0);

    /* ZCR uses a fixed scale and saturates rather than wrapping. */
    std::memset(&r, 0, sizeof(r));
    r.zcr[0] = ZCR_FULL_SCALE;
    r.zcr[1] = (uint16_t)(ZCR_FULL_SCALE * 4u);
    r.zcr[2] = (uint16_t)(ZCR_FULL_SCALE / 2u);
    vpl_normalize(&r, a);
    CHECK(a[0 * FEATURES_PER_SEGMENT + FEATURE_IDX_ZCR] == 255);
    CHECK(a[1 * FEATURES_PER_SEGMENT + FEATURE_IDX_ZCR] == 255);
    CHECK(a[2 * FEATURES_PER_SEGMENT + FEATURE_IDX_ZCR] == 127);
    return 0;
}

/* ------------------------------------------------------ distance/template */

static int test_distance(void)
{
    uint8_t a[FEATURE_COUNT], b[FEATURE_COUNT];
    std::memset(a, 0, sizeof(a));
    std::memset(b, 0, sizeof(b));
    CHECK(vpl_distance(a, b) == 0);

    std::memset(b, 255, sizeof(b));
    CHECK(vpl_distance(a, b) == DISTANCE_MAX);
    CHECK(vpl_distance(b, a) == DISTANCE_MAX); /* symmetric */

    /* One energy feature off by 10 must count 3x, one band feature 1x. */
    std::memset(b, 0, sizeof(b));
    b[FEATURE_IDX_ENERGY] = 10;
    CHECK(vpl_distance(a, b) == 10u * WEIGHT_ENERGY);
    std::memset(b, 0, sizeof(b));
    b[FEATURE_IDX_BAND0] = 10;
    CHECK(vpl_distance(a, b) == 10u * WEIGHT_BAND);
    std::memset(b, 0, sizeof(b));
    b[FEATURE_IDX_ZCR] = 10;
    CHECK(vpl_distance(a, b) == 10u * WEIGHT_ZCR);

    /* The weight pattern must repeat for every segment, not just the first. */
    std::memset(b, 0, sizeof(b));
    b[7u * FEATURES_PER_SEGMENT + FEATURE_IDX_ENERGY] = 10;
    CHECK(vpl_distance(a, b) == 10u * WEIGHT_ENERGY);
    return 0;
}

static int test_template(void)
{
    uint8_t v0[FEATURE_COUNT], v1[FEATURE_COUNT], v2[FEATURE_COUNT], out[FEATURE_COUNT];
    std::memset(v0, 10, sizeof(v0));
    std::memset(v1, 20, sizeof(v1));
    std::memset(v2, 31, sizeof(v2));
    const uint8_t *vs[3] = { v0, v1, v2 };

    vpl_template_average(vs, 3, out);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(out[i] == 20); /* 61/3 = 20.33 */

    /* Rounding is to nearest, so 62/3 = 20.67 must land on 21, not 20. */
    std::memset(v2, 32, sizeof(v2));
    vpl_template_average(vs, 3, out);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(out[i] == 21);

    /* No overflow at the top of the range: 3 x 255 = 765 needs 16 bits. */
    std::memset(v0, 255, sizeof(v0));
    std::memset(v1, 255, sizeof(v1));
    std::memset(v2, 255, sizeof(v2));
    vpl_template_average(vs, 3, out);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(out[i] == 255);
    return 0;
}

static int test_threshold(void)
{
    uint8_t v0[FEATURE_COUNT], v1[FEATURE_COUNT], v2[FEATURE_COUNT], tmpl[FEATURE_COUNT];
    const uint8_t *vs[3] = { v0, v1, v2 };

    /* Three identical utterances: spread is 0, so only the floor survives. */
    std::memset(v0, 100, sizeof(v0));
    std::memset(v1, 100, sizeof(v1));
    std::memset(v2, 100, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    CHECK(vpl_threshold(vs, 3, tmpl) == THRESHOLD_FLOOR);

    /* A known spread must produce floor + 2 x spread. */
    std::memset(v0, 100, sizeof(v0));
    std::memset(v1, 100, sizeof(v1));
    std::memset(v2, 100, sizeof(v2));
    v2[FEATURE_IDX_BAND0] = 130; /* +30 on a weight-1 feature */
    vpl_template_average(vs, 3, tmpl);
    uint32_t spread = 0;
    for (int i = 0; i < 3; ++i) {
        uint32_t d = vpl_distance(vs[i], tmpl);
        if (d > spread) spread = d;
    }
    CHECK(vpl_threshold(vs, 3, tmpl) == spread * 2u + THRESHOLD_FLOOR);

    /* Wildly inconsistent training must clamp instead of accepting everyone. */
    std::memset(v0, 0, sizeof(v0));
    std::memset(v1, 255, sizeof(v1));
    std::memset(v2, 0, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    CHECK(vpl_threshold(vs, 3, tmpl) == THRESHOLD_MAX);
    return 0;
}

/* ------------------------------------------------------------------ FFT */

static int test_fft(void)
{
    int16_t fr[FFT_N], fi[FFT_N];

    /* A pure sinusoid at bin 8 must put its peak at bin 8. */
    for (unsigned n = 0; n < FFT_N; ++n) {
        fr[n] = (int16_t)(8000.0 * std::sin(2.0 * M_PI * 8.0 * n / FFT_N));
        fi[n] = 0;
    }
    fft64(fr, fi);
    unsigned peak = 0;
    uint16_t best = 0;
    for (unsigned k = 1; k < FFT_N / 2; ++k) {
        uint16_t m = fft64_magnitude(fr[k], fi[k]);
        if (m > best) { best = m; peak = k; }
    }
    CHECK(peak == 8);
    /* Scaling: the transform divides by N, so a sine of amplitude A lands at
     * about A/2. Allow the fixed-point and magnitude-approximation slop. */
    CHECK(best > 3200 && best < 4800);

    /* DC in, DC out: a constant must land entirely in bin 0. */
    for (unsigned n = 0; n < FFT_N; ++n) { fr[n] = 4000; fi[n] = 0; }
    fft64(fr, fi);
    CHECK(fft64_magnitude(fr[0], fi[0]) > 3800);
    for (unsigned k = 1; k < FFT_N / 2; ++k) {
        CHECK(fft64_magnitude(fr[k], fi[k]) < 200);
    }

    /* The window must be applied and the pre-scale must not overflow. */
    int16_t src[FRAME_LEN];
    for (unsigned n = 0; n < FRAME_LEN; ++n) {
        src[n] = (int16_t)(511.0 * std::sin(2.0 * M_PI * 12.0 * n / FFT_N));
    }
    fft64_window(src, fr, fi);
    for (unsigned n = 0; n < FRAME_LEN; ++n) CHECK(fi[n] == 0);
    fft64(fr, fi);
    peak = 0; best = 0;
    for (unsigned k = 1; k < FFT_N / 2; ++k) {
        uint16_t m = fft64_magnitude(fr[k], fi[k]);
        if (m > best) { best = m; peak = k; }
    }
    CHECK(peak == 12);

    /* Magnitude approximation: exact on the axes, within 12% elsewhere. */
    CHECK(fft64_magnitude(1000, 0) == 1000);
    CHECK(fft64_magnitude(0, -1000) == 1000);
    double exact = std::sqrt(1000.0 * 1000.0 + 1000.0 * 1000.0);
    double approx = fft64_magnitude(1000, 1000);
    CHECK(std::fabs(approx - exact) / exact < 0.12);
    return 0;
}

/* ------------------------------------------------------------------ VAD */

static int test_vad(void)
{
    vad_reset();
    /* A quiet room must never fire. */
    for (int i = 0; i < 200; ++i) {
        CHECK(vad_process_frame(100) == VadEvent::NONE);
    }
    /* A loud burst must fire START on exactly the VAD_START_FRAMES-th frame. */
    for (unsigned i = 1; i < VAD_START_FRAMES; ++i) {
        CHECK(vad_process_frame(20000) == VadEvent::NONE);
    }
    CHECK(vad_process_frame(20000) == VadEvent::START);
    CHECK(vad_state() == VadState::SPEECH_ACTIVE);

    /* A brief dip inside the utterance must not end it. */
    for (unsigned i = 0; i < VAD_END_FRAMES - 1u; ++i) {
        CHECK(vad_process_frame(100) == VadEvent::NONE);
    }
    CHECK(vad_process_frame(20000) == VadEvent::NONE);
    CHECK(vad_state() == VadState::SPEECH_ACTIVE);

    /* A long enough silence must end it. */
    for (unsigned i = 1; i < VAD_END_FRAMES; ++i) {
        CHECK(vad_process_frame(100) == VadEvent::NONE);
    }
    CHECK(vad_process_frame(100) == VadEvent::END);
    CHECK(vad_state() == VadState::SILENCE);

    /* A single loud frame in a quiet room is a click, not speech. */
    vad_reset();
    for (int i = 0; i < 50; ++i) (void)vad_process_frame(100);
    CHECK(vad_process_frame(20000) == VadEvent::NONE);
    CHECK(vad_process_frame(100) == VadEvent::NONE);
    CHECK(vad_state() == VadState::SILENCE);
    return 0;
}

/* ------------------------------------------------- end-to-end recognition */

/** One synthetic "utterance": per segment, a dominant tone and a level. */
struct Utterance {
    double freq[SEGMENT_COUNT];  /* Hz */
    double level[SEGMENT_COUNT]; /* 0..1 of full ADC swing */
};

/** Run a synthetic utterance through the real feature extractor. */
static void extract(const Utterance &u, double gain, int freq_jit, int level_jit,
                    uint8_t *out)
{
    feature_reset();
    int16_t frame[FRAME_LEN];
    double phase = 0.0;
    for (unsigned f = 0; f < CAPTURE_FRAMES; ++f) {
        const unsigned seg = f / FRAMES_PER_SEGMENT;
        const double hz = u.freq[seg] + jitter(freq_jit);
        const double amp = 511.0 * u.level[seg] * gain * (1.0 + jitter(level_jit) / 100.0);
        for (unsigned n = 0; n < FRAME_LEN; ++n) {
            /* Two harmonics plus a little noise: enough spectral structure for
             * the band features to have something to separate. */
            double v = amp * (std::sin(phase) + 0.4 * std::sin(2.0 * phase));
            v += jitter(4);
            phase += 2.0 * M_PI * hz / SAMPLE_RATE_HZ;
            if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
            if (v > 511.0) v = 511.0;
            if (v < -512.0) v = -512.0;
            frame[n] = (int16_t)v;
        }
        (void)feature_process_frame(frame);
    }
    feature_finalize(out);
}

static int test_end_to_end(void)
{
    /* "Authorised speaker saying the passphrase": a fixed sequence of formant-
     * like tones with a plausible loudness contour. */
    const Utterance phrase_a = {
        { 320, 700, 1150, 480, 1800, 900, 2400, 600 },
        { 0.25, 0.85, 0.70, 0.40, 0.95, 0.65, 0.30, 0.15 },
    };
    /* "Same speaker, different phrase": same tone palette, different order and
     * envelope, which is what a wrong passphrase looks like to this system. */
    const Utterance phrase_b = {
        { 600, 2400, 900, 1800, 480, 1150, 700, 320 },
        { 0.15, 0.30, 0.65, 0.95, 0.40, 0.70, 0.85, 0.25 },
    };
    /* "Different speaker, right phrase": same contour, formants shifted up,
     * which is roughly what a shorter vocal tract does. */
    Utterance speaker_c = phrase_a;
    for (unsigned s = 0; s < SEGMENT_COUNT; ++s) speaker_c.freq[s] *= 1.6;

    uint8_t t0[FEATURE_COUNT], t1[FEATURE_COUNT], t2[FEATURE_COUNT];
    uint8_t tmpl[FEATURE_COUNT], probe[FEATURE_COUNT];

    seed(7);
    extract(phrase_a, 1.00, 6, 5, t0);
    extract(phrase_a, 0.90, 6, 5, t1);
    extract(phrase_a, 1.10, 6, 5, t2);
    const uint8_t *vs[3] = { t0, t1, t2 };
    vpl_template_average(vs, 3, tmpl);
    const uint32_t thr = vpl_threshold(vs, 3, tmpl);
    std::printf("  threshold                 = %lu\n", (unsigned long)thr);

    /* Case 1: authorised speaker, correct phrase -> ACCEPT. */
    extract(phrase_a, 1.00, 6, 5, probe);
    uint32_t d = vpl_distance(probe, tmpl);
    std::printf("  case 1 same speaker+phrase = %-6lu %s\n", (unsigned long)d,
                d <= thr ? "ACCEPT" : "REJECT");
    CHECK(d <= thr);

    /* Case 1b: the same utterance at a quarter of the volume must still pass,
     * or the normalisation is not doing its job. */
    extract(phrase_a, 0.25, 6, 5, probe);
    d = vpl_distance(probe, tmpl);
    std::printf("  case 1b quarter volume     = %-6lu %s\n", (unsigned long)d,
                d <= thr ? "ACCEPT" : "REJECT");
    CHECK(d <= thr);

    /* Case 2: authorised speaker, wrong phrase -> REJECT. */
    extract(phrase_b, 1.00, 6, 5, probe);
    d = vpl_distance(probe, tmpl);
    std::printf("  case 2 wrong phrase        = %-6lu %s\n", (unsigned long)d,
                d <= thr ? "ACCEPT" : "REJECT");
    CHECK(d > thr);

    /* Case 3: different speaker, correct phrase -> REJECT. */
    extract(speaker_c, 1.00, 6, 5, probe);
    d = vpl_distance(probe, tmpl);
    std::printf("  case 3 wrong speaker       = %-6lu %s\n", (unsigned long)d,
                d <= thr ? "ACCEPT" : "REJECT");
    CHECK(d > thr);

    /* Case 5/6: silence and broadband noise must not look like the passphrase. */
    feature_reset();
    int16_t frame[FRAME_LEN];
    std::memset(frame, 0, sizeof(frame));
    for (unsigned f = 0; f < CAPTURE_FRAMES; ++f) (void)feature_process_frame(frame);
    feature_finalize(probe);
    d = vpl_distance(probe, tmpl);
    std::printf("  case 5 silence             = %-6lu %s\n", (unsigned long)d,
                d <= thr ? "ACCEPT" : "REJECT");
    CHECK(d > thr);

    feature_reset();
    for (unsigned f = 0; f < CAPTURE_FRAMES; ++f) {
        for (unsigned n = 0; n < FRAME_LEN; ++n) frame[n] = (int16_t)jitter(400);
        (void)feature_process_frame(frame);
    }
    feature_finalize(probe);
    d = vpl_distance(probe, tmpl);
    std::printf("  case 6 noise               = %-6lu %s\n", (unsigned long)d,
                d <= thr ? "ACCEPT" : "REJECT");
    CHECK(d > thr);
    return 0;
}

/* ----------------------------------------------------------------- driver */

int main(void)
{
    struct { const char *name; int (*fn)(void); } tests[] = {
        { "isqrt", test_isqrt },
        { "crc16", test_crc },
        { "normalize", test_normalize },
        { "distance", test_distance },
        { "template average", test_template },
        { "threshold", test_threshold },
        { "fft64", test_fft },
        { "vad", test_vad },
        { "end-to-end recognition", test_end_to_end },
    };
    for (auto &t : tests) {
        std::printf("== %s\n", t.name);
        if (t.fn() != 0) {
            std::printf("\nFAILED in %s\n", t.name);
            return 1;
        }
    }
    std::printf("\nOK - %d checks passed\n", g_checks);
    return 0;
}
