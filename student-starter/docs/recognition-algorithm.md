# Recognition algorithm

> **Student version.** This specifies the pipeline you must build and the
> properties your implementation has to satisfy. It deliberately leaves the
> scaling rules, the distance weights and the threshold formula to you — those
> are the design decisions you will be graded on and asked to defend. See
> [../ASSIGNMENT.md](../ASSIGNMENT.md).

## What this is, and what it is not

The system performs **template-based speaker + passphrase verification**. It
answers one question: *does this 1.6 seconds of audio have the same acoustic
shape as the utterance I was trained on?*

It is **not** speech-to-text. It never recovers the Persian words «پارکینگ باز
شو», it has no phoneme model, no language model and no dictionary. If you
trained it on someone humming a tune, it would accept the tune. The passphrase
matters only because a different phrase produces a different sequence of
spectra and energies over time, and that difference shows up in the distance.

Do not claim otherwise in your report. Read
[the limitations](#limitations) before you write the conclusion.

## Pipeline

```
8 kHz samples
   |
   +-> 64-sample frames (8 ms)
         |
         +-> Hamming window, Q15                     -> fft64_window()   PROVIDED
         +-> 64-point fixed-point FFT                -> fft64()          PROVIDED
         +-> |X[k]| for the bins in each band        -> fft64_magnitude() PROVIDED
         +-> sum x^2          (short-time energy)                        TODO
         +-> sign changes     (zero-crossing count)                      TODO
         |
   +-> accumulate into 8 temporal segments of 25 frames (200 ms each)    TODO
         |
   +-> normalise to 8 segments x 10 features, each 0..255                TODO
         |
   +-> 80-byte feature vector
```

1.6 s at 8 kHz is 12800 samples, which will never fit in 2 KB of SRAM. Nothing
may be stored at sample rate: each frame is folded into the accumulators for
its segment and then discarded.

## Frame and segment arithmetic

| Quantity | Value | Derivation |
|---|---|---|
| Sample rate | 8000 Hz | Timer2 CTC, /8, OCR2A = 249 |
| Frame length | 64 samples | one FFT |
| Frame duration | 8.0 ms | 64 / 8000 |
| Capture window | 1.6 s | given |
| Frames per capture | 200 | 1.6 s / 8 ms |
| Segments | 8 | given |
| Frames per segment | 25 | 200 / 8 |
| Segment duration | 200 ms | 25 × 8 ms |
| FFT bin width | 125 Hz | 8000 / 64 |

## The FFT — provided, do not rewrite

`fft64.cpp` is complete. It is adapted from Bruce Land's ECE4760 fixed-point
FFT (Cornell), itself derived from Tom Roberts' 1989 `fix_fft` as revised by
Malcolm Slaney in 1994. Attribution is at the top of the file and in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

Two properties of it that you need to understand to use it correctly:

**Scaling.** Every butterfly stage divides its output by two, so the result is
the true DFT divided by 64 and no intermediate can exceed the largest input.
That is what makes it overflow-proof in `int16_t`. Because the centred ADC
value only spans ±512, `fft64_window()` pre-scales left by `FFT_INPUT_SHIFT`
= 4 bits; without that, the quiet bins would be quantised away. Worst-case bin
magnitude is then 8192 / 2 = 4096.

**Magnitude.** `fft64_magnitude()` returns `max(|re|,|im|) + min(|re|,|im|)/4`.
The exact `sqrt(re² + im²)` would cost a 32-bit square root per bin per frame.
The approximation is exact on the axes and worth up to +11.8% at 45°. Since the
same approximation is applied at training and at test time, the bias cancels in
the distance.

## Frequency bands — TODO

Bin *k* is centred on *k* × 125 Hz. Bins 0..31 are the usable half.

Fill in `BAND_START` / `BAND_END` in `feature_extract.cpp` and reproduce the
resulting table here, with the frequency range of each band. Constraints:

* **Cover roughly 250 Hz to 3500 Hz.** That is the range that carries speech.
* **Exclude bins 0 and 1.** Bin 0 is DC — the residual of the 2.5 V bias and
  any drift in the divider. Bin 1 is 125 Hz, where mains hum and its harmonics
  live. Neither carries voice.
* **Stay below the anti-alias filter's corner.** The input RC corners at
  3.4 kHz (see [adc.md](adc.md)) and Nyquist is 4 kHz. Above that you are
  measuring a mixture of attenuated signal and whatever aliased in.
* **The bands need not be equal width.** Human frequency resolution is much
  finer at the low end — that is the idea a mel filterbank encodes. Equal-width
  bands spend most of the feature budget above 2 kHz, where speech carries the
  least identifying information. Decide, and justify your decision.

`test/host_tests.cpp` checks that your table is monotone, non-overlapping and
inside the legal range. It does not check that it is a *good* table; that is
what your report is for.

## Features

Ten per segment, eighty per utterance, one unsigned byte each.

| Index | Feature | Raw accumulator | Range before normalisation |
|---|---|---|---|
| 0 | short-time energy | Σ x² over 1600 samples | 0 .. 4.19e8 |
| 1 | zero-crossing count | sign changes over 1600 samples | 0 .. 1600 |
| 2..9 | band level | Σ bin magnitudes over 25 frames | depends on your bands |

Eight bits per feature is enough: the distance metric only needs a ranking, and
the measurement noise between two recordings of the same phrase is far larger
than 1/255 of full scale. It also keeps the EEPROM record small.

Two implementation traps:

* **Zero crossings do not respect frame boundaries.** Carry the sign of the
  last sample of one frame into the next, or you lose one crossing per frame
  boundary — 25 per segment, which is not noise.
* **Check your accumulator widths.** One segment's energy reaches
  1600 × 512² = 4.19 × 10⁸. Work out which type that needs and what it costs on
  an 8-bit core.

## Normalisation — TODO

The three quantities have wildly different units. They must all land on one
0..255 scale, or the distance is dominated by whichever is numerically largest.

Properties your implementation must have, all checked by `test/host_tests.cpp`:

1. **Loudness invariance.** Multiplying every raw accumulator by a constant —
   the same phrase spoken louder — must not change the output. If it does, you
   have built a volume meter, not a lock, and an impostor can pass by shouting.
2. **Silence produces zeros**, with no division by zero on the way.
3. **The full 0..255 range is used.** Otherwise you are throwing away
   resolution the distance metric needs.
4. **Nothing overflows.** `energy` alone reaches 4.2 × 10⁸; think about what
   happens when you multiply that by 255 in a `uint32_t`.

Questions to answer in your report:

* Which quantities should be normalised *per utterance*, and which belong on a
  *fixed absolute* scale? Some of these carry information in their absolute
  value that a per-utterance rescale would destroy — which ones, and why?
* Speech has a very large dynamic range. Does a straight linear scale keep the
  quiet parts distinguishable, or do you need a compressive one first?
  `vpl_isqrt()` is provided if you want it.
* If you normalise a band level, do you scale it against the maximum in *its
  own segment* or against the maximum across the *whole utterance*? What
  happens to a silent segment under each choice?

## Distance — TODO

```
distance = Σ over i of weight(i) · |input[i] − template[i]|
```

Manhattan rather than Euclidean: absolute differences are cheap on an AVR,
there is no square root, and the ranking they produce is exactly what a
threshold test needs.

There are 8 band features per segment and only 2 temporal ones, so with equal
weights the spectrum outvotes the energy and ZCR contour 8:2. Broadly, the
spectrum carries *who* is speaking and the temporal contour carries *what* they
said, and this system has to get both right. Choose a split and defend it.

Work out the largest value your metric can return. You need that number for
your threshold ceiling, and it belongs in `config.h` as a named constant.

## Training — TODO

Three utterances from the same speaker saying the same phrase, averaged into
one template. Two things to get right:

* The sum of three 0..255 values reaches 765, which does not fit in the
  `uint8_t` you are averaging.
* Round to nearest rather than truncating, or repeated retraining drifts the
  template downwards.

You only need to *store* two of the three vectors — the third is the freshly
captured one, which has to exist anyway. That saves 80 bytes.

## Threshold calibration — TODO

The threshold may **not** be a magic number. It has to be measured from how much
the three training utterances disagree, so that a speaker who repeats
themselves consistently gets a tight lock and one who does not gets a looser
one.

A workable shape:

1. measure the spread of the training vectors against the finished template;
2. multiply by a safety margin;
3. add a floor;
4. clamp to a ceiling.

Points to settle, and to explain in your report:

* Should the spread be measured **pairwise between the utterances** or **against
  the template**? One of the two is in the same units as the test-time
  comparison, and the other is roughly twice as large. Which, and why does it
  matter?
* What goes wrong without the floor, when three recordings are nearly
  identical?
* What goes wrong without the ceiling, when they are wildly different?
* What should the device *tell the operator* when the ceiling is hit?

Print the three training-to-template distances at the end of training. They are
the single most useful diagnostic in the system: if they are large, the
recordings disagree and no threshold will rescue the result.

## Limitations

These are properties of the design, not bugs. Be honest about them in your
report — a report that claims more than the system does loses marks.

1. **No time alignment.** Segments are fixed 200 ms windows from the moment the
   VAD fires. Say the phrase 15% faster and every boundary lands somewhere
   different. Dynamic time warping is the standard fix and does not fit in 2 KB
   alongside everything else.
2. **The capture window is fixed at 1.6 s.** A shorter utterance is padded with
   whatever follows it; a longer one is truncated.
3. **Speaker and phrase are entangled.** One distance covers both, so the
   system cannot tell you *why* it rejected.
4. **Eight bands is very coarse** compared with the 32 mel filters plus DCT of
   the Cornell project this is based on.
5. **The VAD trigger is not the start of the word.** A few frames of the leading
   consonant are lost. Consistent between training and testing, so it does not
   bias the comparison — but it is worth knowing.
6. **One template, one speaker.** No impostor model, no score normalisation.
7. **Expect modest accuracy.** The far more sophisticated Cornell system
   reported 77–83% correct unlock. This is an ADC/DAC/PWM teaching platform
   that happens to recognise speech, not a security product.
