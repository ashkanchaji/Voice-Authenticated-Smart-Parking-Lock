#!/usr/bin/env python3
"""Synthesise the DAC validation clip used as the placeholder success sound.

Two parts, chosen so that a student with an oscilloscope can check two
different things without changing the probe:

  1. a 0..63 staircase ramp, which shows every DAC code in order and makes any
     stuck bit, swapped bit or wrong PORTD mask immediately visible;
  2. a 1 kHz sine, which shows the staircase quantisation of a real waveform
     and is easy to measure for frequency and amplitude.

This is NOT the Persian success message. See tools/wav_to_6bit_header.py.
"""

from __future__ import annotations

import argparse
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wavkit  # noqa: E402


def staircase(seconds: float, rate: int, steps: int = 64) -> list[float]:
    """Ascending ramp through all `steps` DAC codes, mapped back to [-1, 1]."""
    n = int(seconds * rate)
    out = []
    for i in range(n):
        code = min(steps - 1, i * steps // max(1, n))
        out.append(code / (steps - 1) * 2.0 - 1.0)
    return out


def tone(seconds: float, rate: int, freq: float, amp: float = 0.9) -> list[float]:
    n = int(seconds * rate)
    return [amp * math.sin(2.0 * math.pi * freq * i / rate) for i in range(n)]


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-o", "--output", default="test_tone.wav")
    p.add_argument("--rate", type=int, default=wavkit.FIRMWARE_RATE_HZ)
    p.add_argument("--ramp-seconds", type=float, default=0.10)
    p.add_argument("--tone-seconds", type=float, default=0.20)
    p.add_argument("--tone-hz", type=float, default=1000.0)
    args = p.parse_args(argv)

    samples = (staircase(args.ramp_seconds, args.rate)
               + tone(args.tone_seconds, args.rate, args.tone_hz))
    wavkit.write_wav(args.output, samples, args.rate)
    print(f"{args.output}: {len(samples)} samples, {len(samples)/args.rate:.3f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
