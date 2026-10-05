/* Host-side checks for the parts of the firmware that are pure arithmetic.
 *
 * These are the SPECIFICATION, written as runnable assertions. They do not
 * check that you implemented the reference solution - they check the
 * properties any correct implementation must have. Every one of them fails
 * against the starter stubs; making them pass is a good order to work in,
 * and it is much faster than debugging scaling bugs through a Proteus
 * terminal.
 *
 * The same source files compile for the AVR and for your laptop. That only
 * stays true if recognizer.cpp, feature_extract.cpp, fft64.cpp and vad.cpp
 * never touch an AVR register - keep the peripheral code out of them.
 *
 * Build and run:  ./test/run.sh
 *
 * TODO: add your own cases. The distance and threshold tests here deliberately
 * do not pin down the weights or the safety margin, because those are your
 * design decisions - but you should be testing the values you chose.
 */

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "recognizer.h"
#include "fft64.h"
#include "feature_extract.h"
#include "vad.h"

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        ++g_checks;                                                     \
        if (!(cond)) {                                                  \
            ++g_failures;                                               \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

/* ------------------------------------------- provided helpers (should pass) */

static void test_provided_helpers(void)
{
    /* vpl_isqrt and vpl_crc16 come finished; if these fail, something is very
     * wrong with your build rather than with your code. */
    CHECK(vpl_isqrt(0) == 0);
    CHECK(vpl_isqrt(65025) == 255);
    CHECK(vpl_isqrt(0xFFFFFFFFul) == 65535);
    for (uint32_t v = 0; v < 20000u; v += 13u) {
        uint32_t r = vpl_isqrt(v);
        CHECK(r * r <= v && (r + 1u) * (r + 1u) > v);
    }
    const char *s = "123456789";
    CHECK(vpl_crc16((const uint8_t *)s, 9) == 0x29B1u); /* published check value */
}

static void test_fft_provided(void)
{
    /* fft64 is provided complete. These cases show you what it does, and give
     * you a reference point when your band energies look wrong. */
    int16_t fr[FFT_N], fi[FFT_N];
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
    CHECK(peak == 8);        /* a tone at bin 8 peaks at bin 8 */
    CHECK(best > 3200 && best < 4800); /* the transform divides by N */

    fft64_window(fr, fr, fi);
    for (unsigned n = 0; n < FRAME_LEN; ++n) CHECK(fi[n] == 0);
}

/* -------------------------------------------------- your code (TODO tests) */

static void test_band_table(void)
{
    /* Your band table must cover the speech range without straying into DC,
     * mains hum or the region above Nyquist. */
    for (unsigned b = 0; b < BAND_COUNT; ++b) {
        const uint8_t lo = feature_band_start(b);
        const uint8_t hi = feature_band_end(b);
        CHECK(lo >= 2);            /* bin 0 is DC, bin 1 is 125 Hz */
        CHECK(hi <= FFT_N / 2 - 1);/* nothing above the usable half */
        CHECK(lo <= hi);
        if (b > 0) {
            CHECK(lo > feature_band_end(b - 1)); /* bands must not overlap */
        }
    }
    CHECK(feature_band_start(0) * FFT_BIN_HZ >= 200u);
    CHECK(feature_band_end(BAND_COUNT - 1) * FFT_BIN_HZ <= 3625u);
}

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

static void test_normalize(void)
{
    RawFeatures r;
    uint8_t a[FEATURE_COUNT], b[FEATURE_COUNT];

    /* Silence in, silence out - and no division by zero on the way. */
    std::memset(&r, 0, sizeof(r));
    vpl_normalize(&r, a);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(a[i] == 0);

    /* Loudness invariance. This is the property that stops the lock from being
     * a volume meter, and it is worth marks on its own. */
    fill_raw(&r, 1);
    vpl_normalize(&r, a);
    fill_raw(&r, 64);
    vpl_normalize(&r, b);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) {
        CHECK(std::abs((int)a[i] - (int)b[i]) <= 1);
    }

    /* The whole 0..255 range should be in use, or you are throwing away
     * resolution the distance metric needs. */
    fill_raw(&r, 1);
    vpl_normalize(&r, a);
    int hi = 0;
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) if (a[i] > hi) hi = a[i];
    CHECK(hi >= 250);
}

static void test_distance(void)
{
    uint8_t a[FEATURE_COUNT], b[FEATURE_COUNT];
    std::memset(a, 100, sizeof(a));

    std::memcpy(b, a, sizeof(b));
    CHECK(vpl_distance(a, b) == 0); /* identical vectors are at distance 0 */

    b[3] = 150;
    const uint32_t d1 = vpl_distance(a, b);
    CHECK(d1 > 0);
    CHECK(d1 == vpl_distance(b, a)); /* symmetric */

    b[3] = 200;
    CHECK(vpl_distance(a, b) > d1); /* monotone in the difference */

    /* The weight pattern must repeat for every segment. Feature 3 of segment 0
     * and feature 3 of segment 7 are the same kind of feature, so an identical
     * error in each must cost the same. */
    std::memset(b, 100, sizeof(b));
    b[3] = 150;
    const uint32_t seg0 = vpl_distance(a, b);
    std::memset(b, 100, sizeof(b));
    b[7u * FEATURES_PER_SEGMENT + 3u] = 150;
    CHECK(vpl_distance(a, b) == seg0);
}

static void test_template_and_threshold(void)
{
    uint8_t v0[FEATURE_COUNT], v1[FEATURE_COUNT], v2[FEATURE_COUNT], tmpl[FEATURE_COUNT];
    const uint8_t *vs[3] = { v0, v1, v2 };

    /* Averaging three identical vectors must reproduce them exactly. */
    std::memset(v0, 200, sizeof(v0));
    std::memset(v1, 200, sizeof(v1));
    std::memset(v2, 200, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(tmpl[i] == 200);

    /* No overflow at the top of the range: 3 * 255 = 765 does not fit in 8 bits. */
    std::memset(v0, 255, sizeof(v0));
    std::memset(v1, 255, sizeof(v1));
    std::memset(v2, 255, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(tmpl[i] == 255);

    /* The average must lie between the inputs, not outside them. */
    std::memset(v0, 10, sizeof(v0));
    std::memset(v1, 20, sizeof(v1));
    std::memset(v2, 30, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    for (unsigned i = 0; i < FEATURE_COUNT; ++i) CHECK(tmpl[i] >= 10 && tmpl[i] <= 30);

    /* Three identical utterances have zero spread, but the threshold must not
     * be zero - the speaker has to be able to pass on their next attempt. */
    std::memset(v0, 100, sizeof(v0));
    std::memset(v1, 100, sizeof(v1));
    std::memset(v2, 100, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    const uint32_t tight = vpl_threshold(vs, 3, tmpl);
    CHECK(tight > 0);

    /* Inconsistent utterances must give a looser threshold... */
    std::memset(v2, 160, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    const uint32_t loose = vpl_threshold(vs, 3, tmpl);
    CHECK(loose > tight);

    /* ...but not an infinitely loose one. Work out the largest distance your
     * metric can return; the threshold must stay well under it, or every
     * impostor is admitted. */
    std::memset(v0, 0, sizeof(v0));
    std::memset(v1, 255, sizeof(v1));
    std::memset(v2, 0, sizeof(v2));
    vpl_template_average(vs, 3, tmpl);
    uint8_t far0[FEATURE_COUNT], far1[FEATURE_COUNT];
    std::memset(far0, 0, sizeof(far0));
    std::memset(far1, 255, sizeof(far1));
    CHECK(vpl_threshold(vs, 3, tmpl) < vpl_distance(far0, far1) / 2u);
}

static void test_vad(void)
{
    vad_reset();
    /* A quiet room must never fire. */
    for (int i = 0; i < 200; ++i) CHECK(vad_process_frame(100) == VadEvent::NONE);

    /* Speech must eventually be detected - but not on the very first loud
     * frame, because one loud frame is a door slamming. */
    CHECK(vad_process_frame(20000) == VadEvent::NONE);
    bool started = false;
    for (int i = 0; i < 20 && !started; ++i) {
        started = vad_process_frame(20000) == VadEvent::START;
    }
    CHECK(started);
    CHECK(vad_state() == VadState::SPEECH_ACTIVE);

    /* Silence must eventually end it, and not on the first quiet frame either -
     * that is the gap between two words. */
    CHECK(vad_process_frame(100) == VadEvent::NONE);
    bool ended = false;
    for (int i = 0; i < 100 && !ended; ++i) {
        ended = vad_process_frame(100) == VadEvent::END;
    }
    CHECK(ended);
    CHECK(vad_state() == VadState::SILENCE);
}

static void test_capture_length(void)
{
    /* The extractor must consume exactly CAPTURE_FRAMES frames and then say so. */
    int16_t frame[FRAME_LEN];
    for (unsigned n = 0; n < FRAME_LEN; ++n) {
        frame[n] = (int16_t)(300.0 * std::sin(2.0 * M_PI * 5.0 * n / FRAME_LEN));
    }
    feature_reset();
    CHECK(feature_frames_done() == 0);
    for (unsigned f = 0; f + 1 < CAPTURE_FRAMES; ++f) {
        CHECK(feature_process_frame(frame) == false);
    }
    CHECK(feature_process_frame(frame) == true);
    CHECK(feature_frames_done() == CAPTURE_FRAMES);

    /* A loud frame must produce a larger magnitude than a quiet one. */
    int16_t quiet[FRAME_LEN];
    for (unsigned n = 0; n < FRAME_LEN; ++n) quiet[n] = (int16_t)(frame[n] / 8);
    CHECK(feature_frame_magnitude(frame) > feature_frame_magnitude(quiet));
    CHECK(feature_frame_magnitude(frame) > 0);
}

/* ----------------------------------------------------------------- driver */

int main(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "provided helpers (isqrt, crc16)", test_provided_helpers },
        { "provided FFT", test_fft_provided },
        { "band table", test_band_table },
        { "feature normalisation", test_normalize },
        { "distance metric", test_distance },
        { "template + threshold", test_template_and_threshold },
        { "voice activity detection", test_vad },
        { "capture length + frame magnitude", test_capture_length },
    };
    for (auto &t : tests) {
        const int before = g_failures;
        std::printf("== %s\n", t.name);
        t.fn();
        if (g_failures == before) std::printf("  ok\n");
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
