#!/usr/bin/env python3
"""Tests for the Python tooling. Run: python3 -m unittest discover tools/tests"""

import math
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

import make_test_tone  # noqa: E402
import prepare_voice_dataset  # noqa: E402
import wav_to_6bit_header  # noqa: E402
import wavkit  # noqa: E402


class TestQuantize6Bit(unittest.TestCase):
    def test_endpoints_and_midpoint(self):
        # The R-2R ladder is unipolar: -1.0 must be code 0, +1.0 code 63, and
        # digital silence must land on the mid-scale boundary.
        self.assertEqual(wavkit.quantize_6bit([-1.0, 0.0, 1.0]), [0, 32, 63])

    def test_clips_instead_of_wrapping(self):
        # A wrapped sample would be a full-scale glitch in the audio and, on a
        # real speaker, an audible click.
        self.assertEqual(wavkit.quantize_6bit([-5.0, 5.0]), [0, 63])

    def test_monotonic(self):
        codes = wavkit.quantize_6bit([i / 100.0 - 1.0 for i in range(201)])
        self.assertEqual(codes, sorted(codes))
        self.assertEqual(min(codes), 0)
        self.assertEqual(max(codes), 63)

    def test_quantisation_error_bounded(self):
        # One 6-bit code is 2/64 of full scale, so the round-trip error must
        # never exceed half of that.
        for i in range(0, 2001):
            x = i / 1000.0 - 1.0
            code = wavkit.quantize_6bit([x])[0]
            back = code / 31.5 - 1.0
            self.assertLessEqual(abs(back - x), 1.0 / 31.5 / 2 + 1e-9)


class TestSignalHelpers(unittest.TestCase):
    def test_normalize_hits_target_peak(self):
        out = wavkit.normalize([0.1, -0.2, 0.05], peak=0.9)
        self.assertAlmostEqual(max(abs(s) for s in out), 0.9)

    def test_normalize_leaves_silence_alone(self):
        self.assertEqual(wavkit.normalize([0.0, 0.0]), [0.0, 0.0])

    def test_remove_dc(self):
        out = wavkit.remove_dc([1.0, 1.0, 3.0])
        self.assertAlmostEqual(sum(out) / len(out), 0.0)

    def test_resample_changes_length_by_the_rate_ratio(self):
        src = [math.sin(2 * math.pi * 100 * i / 16000) for i in range(16000)]
        out = wavkit.resample(src, 16000, 8000)
        self.assertEqual(len(out), 8000)

    def test_resample_preserves_a_low_tone(self):
        # A 100 Hz tone is far below both Nyquist limits, so decimating must
        # not change its amplitude appreciably.
        src = [math.sin(2 * math.pi * 100 * i / 16000) for i in range(16000)]
        out = wavkit.resample(src, 16000, 8000)
        self.assertGreater(max(out), 0.98)

    def test_lowpass_attenuates_above_cutoff(self):
        rate = 8000
        hi = [math.sin(2 * math.pi * 3500 * i / rate) for i in range(4000)]
        out = wavkit.lowpass(hi, rate, 500)
        self.assertLess(max(abs(s) for s in out[100:-100]), 0.2)

    def test_silence_length(self):
        self.assertEqual(len(wavkit.silence(0.25, 8000)), 2000)


class TestWavRoundTrip(unittest.TestCase):
    def test_write_then_read(self):
        src = [math.sin(2 * math.pi * 440 * i / 8000) * 0.5 for i in range(800)]
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "t.wav")
            wavkit.write_wav(path, src, 8000)
            back, rate = wavkit.read_wav(path)
            self.assertEqual(rate, 8000)
            self.assertEqual(len(back), len(src))
            for a, b in zip(src, back):
                self.assertLess(abs(a - b), 1e-4)


class TestHeaderGeneration(unittest.TestCase):
    def _make_wav(self, d, seconds=0.05, rate=8000):
        path = os.path.join(d, "in.wav")
        n = int(seconds * rate)
        wavkit.write_wav(path, [math.sin(2 * math.pi * 440 * i / rate) for i in range(n)], rate)
        return path

    def test_generates_compilable_looking_header(self):
        with tempfile.TemporaryDirectory() as d:
            src = self._make_wav(d)
            out = os.path.join(d, "success_audio.h")
            self.assertEqual(wav_to_6bit_header.main([src, "-o", out]), 0)
            text = open(out).read()
            self.assertIn("#ifndef SUCCESS_AUDIO_H", text)
            self.assertIn("#define SUCCESS_AUDIO_LEN 400u", text)
            self.assertIn("static const uint8_t SUCCESS_AUDIO_PCM6[400] PROGMEM", text)
            self.assertIn("#define SUCCESS_AUDIO_IS_PLACEHOLDER 0", text)

    def test_placeholder_flag_is_visible_in_the_header(self):
        with tempfile.TemporaryDirectory() as d:
            src = self._make_wav(d)
            out = os.path.join(d, "success_audio.h")
            wav_to_6bit_header.main([src, "-o", out, "--placeholder"])
            text = open(out).read()
            self.assertIn("#define SUCCESS_AUDIO_IS_PLACEHOLDER 1", text)
            self.assertIn("NOT THE PERSIAN SUCCESS MESSAGE", text)

    def test_flash_guard_refuses_oversized_audio(self):
        # Silently generating 30 KB of PROGMEM would fail at link time with a
        # confusing message; failing here says exactly what went wrong.
        with tempfile.TemporaryDirectory() as d:
            src = self._make_wav(d, seconds=3.0)
            out = os.path.join(d, "success_audio.h")
            self.assertEqual(wav_to_6bit_header.main([src, "-o", out, "--max-seconds", "1.0"]), 2)
            self.assertFalse(os.path.exists(out))

    def test_resamples_to_the_firmware_rate(self):
        with tempfile.TemporaryDirectory() as d:
            src = self._make_wav(d, seconds=0.1, rate=44100)
            out = os.path.join(d, "success_audio.h")
            wav_to_6bit_header.main([src, "-o", out])
            self.assertIn("#define SUCCESS_AUDIO_RATE_HZ 8000u", open(out).read())
            self.assertIn("#define SUCCESS_AUDIO_LEN 800u", open(out).read())

    def test_all_codes_are_in_range(self):
        with tempfile.TemporaryDirectory() as d:
            src = self._make_wav(d)
            out = os.path.join(d, "success_audio.h")
            wav_to_6bit_header.main([src, "-o", out])
            body = open(out).read().split("PROGMEM = {")[1].split("};")[0]
            codes = [int(c) for c in body.replace("\n", "").split(",") if c.strip()]
            self.assertEqual(len(codes), 400)
            self.assertTrue(all(0 <= c <= 63 for c in codes))


class TestTestTone(unittest.TestCase):
    def test_staircase_covers_every_dac_code(self):
        ramp = make_test_tone.staircase(0.1, 8000)
        codes = set(wavkit.quantize_6bit(ramp))
        self.assertEqual(len(codes), 64)
        self.assertEqual(min(codes), 0)
        self.assertEqual(max(codes), 63)

    def test_tone_frequency(self):
        t = make_test_tone.tone(0.1, 8000, 1000.0)
        crossings = sum(1 for i in range(1, len(t)) if (t[i - 1] < 0) != (t[i] < 0))
        # 1 kHz over 0.1 s is 100 cycles, i.e. 200 zero crossings.
        self.assertAlmostEqual(crossings, 200, delta=2)


class TestDatasetPreparation(unittest.TestCase):
    def test_concatenates_with_gaps_and_lead_in(self):
        with tempfile.TemporaryDirectory() as d:
            clips = []
            for i in range(3):
                p = os.path.join(d, f"c{i}.wav")
                wavkit.write_wav(p, [0.5] * 8000, 8000)  # 1.0 s each
                clips.append(p)
            out = os.path.join(d, "scenario.wav")
            rc = prepare_voice_dataset.main(
                ["--train", *clips, "-o", out, "--gap", "5.0", "--lead-in", "1.0"])
            self.assertEqual(rc, 0)
            samples, rate = wavkit.read_wav(out)
            self.assertEqual(rate, 8000)
            # 1 s lead-in + 3 x (1 s clip + 5 s gap) = 19 s.
            self.assertEqual(len(samples), int(19.0 * 8000))

    def test_levels_are_equalised_across_clips(self):
        # An impostor recorded louder than the authorised speaker would make
        # the test meaningless, so the tool must flatten the levels.
        with tempfile.TemporaryDirectory() as d:
            quiet = os.path.join(d, "q.wav")
            loud = os.path.join(d, "l.wav")
            wavkit.write_wav(quiet, [0.05 * math.sin(i) for i in range(8000)], 8000)
            wavkit.write_wav(loud, [0.90 * math.sin(i) for i in range(8000)], 8000)
            out = os.path.join(d, "s.wav")
            prepare_voice_dataset.main(["--test", quiet, loud, "-o", out, "--gap", "5.0"])
            samples, _ = wavkit.read_wav(out)
            first = samples[8000:16000]
            second = samples[8000 + 8000 + 40000:8000 + 16000 + 40000]
            self.assertAlmostEqual(max(map(abs, first)), max(map(abs, second)), delta=0.02)


if __name__ == "__main__":
    unittest.main()
