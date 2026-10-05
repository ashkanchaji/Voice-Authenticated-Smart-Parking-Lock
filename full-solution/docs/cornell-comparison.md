# Comparison with the Cornell ECE4760 Speech Lock

The source of the idea:

> Speech Lock — William Salcedo, Rafael Ramos, Rene Lorenzo, ECE 4760 Final
> Project, Cornell University, Fall 2014.
> <https://people.ece.cornell.edu/land/courses/ece4760/FinalProjects/f2014/wjs253_rrs72_ral255/speech_lock_webpage/speech_lock_webpage/webpage.html>

That project is a speaker-dependent word recogniser built on a full MFCC front
end and vector quantisation. This one is a template matcher built to teach
ADC, DAC and PWM on a chip with a quarter of the RAM. The differences are all
consequences of that.

## Side by side

| | Cornell Speech Lock | This project |
|---|---|---|
| MCU | ATmega1284P | ATmega328P (Arduino Uno R3) |
| SRAM | 16 KB | **2 KB** |
| Flash | 128 KB | 32 KB |
| Sample rate | ~9.6 kHz | 8.0 kHz |
| FFT | 128-point fixed point | **64-point fixed point** |
| Fixed-point format | Q8.8 with inline-assembler multiply | Q15 twiddles, plain C multiply |
| Spectral front end | 32 triangular mel filters, log, 32-point DCT | **8 linear-frequency bands, no DCT** |
| Feature vector | 64 cepstral coefficients (2–16 from each of 16 frames) | **80 values: 8 segments × (energy, ZCR, 8 bands)** |
| Model | vector quantisation, LBG, 8 centroids | **arithmetic mean of 3 utterances** |
| Distance | Euclidean, per-vector nearest centroid, shift-based sqrt | **weighted Manhattan, no sqrt** |
| Threshold | fixed constant (7500, or 15000 for a harder word) | **derived from training spread, with floor and ceiling** |
| Persistence | none (retrain on every reset) | **EEPROM, CRC-checked** |
| Output | LCD + LEDs | **servo gate (PWM) + spoken message (R-2R DAC) + LEDs** |
| Input filter | LM358 stage + MAX7401 8th-order switched-capacitor LPF | passive RC, first-order |
| Environment | physical board | **Proteus simulation** |

## What was taken, and how it changed

**The fixed-point FFT.** This is the one substantial piece of adapted code, and
it is credited in `src/fft64.cpp` and
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Bruce Land's ECE4760
version descends from Tom Roberts' 1989 `fix_fft` as revised by Malcolm Slaney
in 1994. Four changes:

1. **128 → 64 points.** One FFT now covers exactly one 8 ms analysis frame, and
   the working buffers cost 256 B instead of 512 B. On 2 KB that is 12% of all
   the RAM in the machine.
2. **Twiddles to `PROGMEM`.** The original builds its sine table at boot with
   floating-point `sin()`, spending 256 B of SRAM and dragging in libm. Here
   `tools/gen_tables.py` emits a Q15 table into flash: 128 B of flash, 0 B of
   SRAM.
3. **Q8.8 assembler → Q15 C.** The `multfix` inline-assembler macro is faster,
   but the C version compiles for the host too, which is what lets the entire
   recognition chain be unit-tested off-target. Measured cost of window + FFT
   is 1.31 ms against an 8 ms budget, so the speed was not needed.
4. **Explicit bounds.** The per-stage 1/2 scaling and the `FFT_INPUT_SHIFT`
   pre-scale are documented with the resulting worst-case magnitudes, so the
   "no overflow" claim is checkable rather than inherited.

Nothing else was ported. The mel filterbank, the DCT, the LBG quantiser and the
shifting square root were all read and then deliberately not used.

## Why the front end was replaced rather than shrunk

**MFCC does not fit, and would not pay for itself here.** Cornell keeps 16
frames × 32 filter outputs alive to run a 32-point DCT over them. Even in 8-bit
that is 512 B before the DCT scratch, on top of a 512 B FFT working set and the
frame matrix — roughly the entire SRAM of an ATmega328P. The mel weighting
table and the DCT cosine table are further flash. And the payoff of a DCT is
decorrelating the filterbank outputs so a Euclidean metric behaves; with eight
bands and a Manhattan metric there is much less correlation to remove.

**LBG vector quantisation solves a problem this project does not have.**
Codebook training is iterative: repeatedly assign 64 vectors to 8 centroids,
recompute, split, converge. That needs all the training vectors resident at
once and an unbounded number of passes. It buys robustness to *where in the
utterance* a sound occurs, because a codebook is order-free. But an order-free
model is exactly the wrong thing for a passphrase lock: "باز شو پارکینگ" would
quantise to nearly the same codebook as «پارکینگ باز شو». The eight fixed
temporal segments here are deliberately order-*sensitive*, and one arithmetic
mean of three vectors is 80 bytes and one pass.

**A fixed threshold of 7500 is a magic number.** The Cornell write-up says as
much: it had to be raised to about 15000 for a more complex word. That is a
per-word constant discovered by experiment and hard-coded. Deriving the
threshold from the spread of the training utterances makes it adapt to the
speaker and the phrase automatically, and turns the calibration into something
a student can explain rather than memorise.

**No square root at all.** Cornell needed one for the Euclidean metric and
implemented a shift-and-subtract nth root to avoid `sqrt()`. Manhattan distance
removes the need entirely. (There is still one integer square root in this
project, in the energy normalisation, but it runs 8 times per utterance rather
than 64 times inside the distance loop.)

## Why this architecture suits an ADC/DAC/PWM course better

The assignment's stated goal is that ADC, DAC and PWM stay the visible subject
and the DSP stay context. Judged against that:

* **The Cornell pipeline hides the peripherals.** Mel filterbanks, log
  compression, DCT and codebook training are three lectures of signal
  processing. A student who spent their time there would learn DSP and
  configure the ADC almost incidentally.
* **This pipeline is four ideas, each one bar chart wide.** Energy, zero
  crossings, band energies, and distance between two vectors. All four can be
  plotted and argued about in an afternoon, which leaves the term for the
  register-level work.
* **Every stage is observable on the Proteus oscilloscope.** The biased input
  at A0, the servo pulse widening from 1 ms to 2 ms on D9, the 64-step
  staircase at the ladder output. Cornell's LCD tells you the answer; a scope
  trace shows you the mechanism. See [proteus-testing.md](proteus-testing.md).
* **The output side actually exercises two peripherals.** Cornell's result is
  four LEDs and an LCD line - no DAC, and no PWM beyond the filter's clock.
  Here a match drives a 50 Hz hardware PWM *and* an 8 kHz interrupt-driven DAC
  stream, concurrently, from two different timers. That concurrency is the
  lesson: neither needs the CPU while it happens.
* **The whole recogniser is host-testable.** Because the DSP modules touch no
  AVR registers, `test/host_tests.cpp` runs the real feature extractor over
  synthetic utterances on a laptop. Cornell's is entangled with the LCD and
  UART code and can only be exercised on hardware.

## What was lost

Honesty about the trade:

* **Accuracy.** Cornell reported 77–83% correct unlock for the intended
  speaker. This system's front end is far coarser; expect less.
* **Robustness to timing.** A codebook does not care when a sound occurred.
  Fixed 200 ms segments care a great deal, so speaking 15% faster moves every
  boundary. See the limitations in
  [recognition-algorithm.md](recognition-algorithm.md#limitations).
* **Spectral resolution.** 8 linear bands against 32 mel filters, and 125 Hz
  bins against Cornell's ~75 Hz.
* **Input conditioning.** A first-order RC against an 8th-order switched-
  capacitor filter. Tolerable with a band-limited WAV source in simulation;
  not with a live microphone.

None of those matter for teaching ADC, DAC and PWM. All of them would matter if
this were a product.
