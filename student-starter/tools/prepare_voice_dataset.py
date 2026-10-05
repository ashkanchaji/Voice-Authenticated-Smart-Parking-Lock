#!/usr/bin/env python3
"""Build a single `scenario.wav` for the Proteus WAV/audio generator.

Proteus feeds the ATmega328P's A0 pin from one audio source, so the whole test
session - three training utterances plus the four recognition cases - has to be
one continuous file with silence between the utterances. This tool does the
resampling, mono downmix, level normalisation, silence insertion and
concatenation, and prints a timeline so you know when to press the training
button during the simulation.

    python3 tools/prepare_voice_dataset.py \
        --train assets/train1.wav assets/train2.wav assets/train3.wav \
        --test assets/correct.wav assets/wrong_phrase.wav assets/wrong_speaker.wav \
        -o assets/scenario.wav

Every clip is normalised to the same peak on purpose: if the authorised
recording were simply louder than the impostor's, the lock would be measuring
volume, not identity. The firmware normalises its features too, but keeping the
stimulus fair means a pass is a real pass.
"""

from __future__ import annotations

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wavkit  # noqa: E402


def load_clip(path: str, rate: int, peak: float, no_normalize: bool) -> list[float]:
    samples, src_rate = wavkit.read_wav(path)
    if src_rate > rate:
        samples = wavkit.lowpass(samples, src_rate, rate * 0.45)
    samples = wavkit.resample(samples, src_rate, rate)
    samples = wavkit.remove_dc(samples)
    if not no_normalize:
        samples = wavkit.normalize(samples, peak)
    return samples


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--train", nargs="*", default=[],
                   help="training utterances, authorised speaker saying the passphrase")
    p.add_argument("--test", nargs="*", default=[],
                   help="recognition test utterances, in the order you want them played")
    p.add_argument("-o", "--output", default="scenario.wav")
    p.add_argument("--rate", type=int, default=wavkit.FIRMWARE_RATE_HZ)
    p.add_argument("--gap", type=float, default=5.0,
                   help="seconds of silence between utterances (must exceed the "
                        "1.6 s capture window plus the 3 s gate-open time)")
    p.add_argument("--lead-in", type=float, default=1.0,
                   help="seconds of silence before the first utterance, so the "
                        "firmware's noise-floor estimator can settle")
    p.add_argument("--peak", type=float, default=0.90)
    p.add_argument("--no-normalize", action="store_true")
    args = p.parse_args(argv)

    clips = [(os.path.basename(f), "train", f) for f in args.train]
    clips += [(os.path.basename(f), "test", f) for f in args.test]
    if not clips:
        print("error: give at least one --train or --test file", file=sys.stderr)
        return 2

    out = wavkit.silence(args.lead_in, args.rate)
    timeline = []
    for name, kind, path in clips:
        start = len(out) / args.rate
        clip = load_clip(path, args.rate, args.peak, args.no_normalize)
        out += clip
        timeline.append((start, len(out) / args.rate, kind, name))
        out += wavkit.silence(args.gap, args.rate)

    wavkit.write_wav(args.output, out, args.rate)

    print(f"{args.output}: {len(out)} samples, {len(out)/args.rate:.2f} s @ {args.rate} Hz")
    print()
    print("  start      end   kind   clip")
    print("  -----   ------   ----   ----")
    for start, end, kind, name in timeline:
        print(f"  {start:5.2f}   {end:6.2f}   {kind:5s}  {name}")
    if args.gap < 4.6:
        print(f"\nwarning: --gap {args.gap}s is below the 4.6 s the firmware needs "
              f"(1.6 s capture + 3.0 s gate open) before it is listening again.",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
