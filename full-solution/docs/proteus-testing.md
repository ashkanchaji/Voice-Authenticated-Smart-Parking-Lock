# Testing in Proteus

This document is the *why*; [../proteus/validation-checklist.md](../proteus/validation-checklist.md)
is the *what to measure*, and [../proteus/wiring.md](../proteus/wiring.md) is
the *how to build it*.

> **The simulation in this repository has not been run.** Proteus was not
> available in the environment where the project was generated, so no `.pdsprj`
> is included and no simulation results are reported. Everything below
> describes what to do and what the numbers should be, derived from the
> firmware and the circuit — not from an observed run.

## Getting audio in

Proteus's **AUDIO** generator plays a WAV file into a net. There is one such
source driving A0, so the entire test session has to be one continuous file:
three training utterances, then the recognition cases, with enough silence
between them.

```sh
python3 tools/prepare_voice_dataset.py \
    --train assets/train1.wav assets/train2.wav assets/train3.wav \
    --test  assets/correct.wav assets/wrong_phrase.wav assets/wrong_speaker.wav \
    -o assets/scenario.wav
```

The default 5 s gap is not arbitrary: the firmware needs 1.6 s to capture plus
3.0 s of gate-open time before it is listening again, so anything under 4.6 s
would have the next utterance arriving while the gate is still open. The tool
warns if you shorten it below that.

The tool also normalises every clip to the same peak. That is deliberate: if
the authorised speaker's recording were simply louder than the impostor's, the
test would prove nothing about identity. The firmware normalises its features
too, but keeping the stimulus fair means a pass is a real pass.

## The three measurements that matter

### ADC — is the signal getting in intact?

Scope channel A on A0. You are looking for audio centred on **2.5 V**, never
touching either rail.

* Centred on 0 V → the coupling capacitor or the bias divider is missing.
* Clipped → the generator amplitude is too high. Clipping creates harmonics
  that land in the analysis bands, so the features end up describing the
  clipping rather than the voice.
* Very small → the features still work (they are normalised), but the
  quantisation noise grows. Aim for roughly 2 V peak-to-peak.

### PWM — is the gate command correct?

Scope channel B on D9. Measure the **period** (20.0 ms) and the **pulse width**
(1.00 ms locked, 2.00 ms unlocked). Both are directly readable on the scope's
cursors and both are exact consequences of `ICR1 = 39999` and `OCR1A` at
2000/4000 with a 0.5 µs tick.

If the horn does not move but the waveform is right, check that PB1 is
configured as an output — the compare unit toggles internally whether or not
the pin is driven.

### DAC — is the conversion linear?

Scope channel C on the ladder output, channel D after the amplifier.

The placeholder clip begins with a **0 → 63 staircase** precisely so this is
easy: 64 equal steps of 78 mV each, one every 125 µs, climbing from 0 V to
4.92 V. Then a 1 kHz sine, which shows the same quantisation on a real
waveform.

| What you see | What it means |
|---|---|
| 64 equal steps | the ladder is correct |
| Steps of unequal height | a 10 k and a 20 k are swapped |
| Ramp runs downward | D2 and D7 are reversed (LSB/MSB swap) |
| One step twice the height of its neighbours | a bit is stuck |
| Staircase looks right, output silent | check `C4`, `RV1` and the LM386 supply |

Comparing channel C with channel D shows what the reconstruction filter does:
the same waveform with the step edges rounded off. That is the "before and
after the filter" view the assignment asks for.

## Watching two peripherals at once

The most interesting single moment in the whole simulation is the three seconds
after `ACCESS GRANTED`. On the scope you should see, simultaneously:

* channel B still producing its 50 Hz servo pulses at 2.00 ms;
* channel C producing an 8 kHz audio staircase;
* the UART trace continuing uninterrupted.

Timer1 holds the gate in hardware and Timer2 streams the DAC. Neither needs the
CPU while it happens, which is exactly why the main loop can stay non-blocking.

## Reading the UART trace

Virtual terminal at 115200 8N1 on D1. A complete successful session:

```
BOOT: VOICE PARKING LOCK
SAMPLE RATE HZ = 8000
CAPTURE FRAMES = 200
FEATURES = 80
AUDIO: PLACEHOLDER TEST TONE, NOT THE PERSIAN MESSAGE
EEPROM INVALID
TRAINING REQUIRED
TRAINING: PRESS BUTTON, THEN SAY THE PASSPHRASE (1/3)
TRAINING SAMPLE 1/3 - SPEAK NOW
SPEECH END
TRAINING: PRESS BUTTON, THEN SAY THE PASSPHRASE (2/3)
TRAINING SAMPLE 2/3 - SPEAK NOW
SPEECH END
TRAINING: PRESS BUTTON, THEN SAY THE PASSPHRASE (3/3)
TRAINING SAMPLE 3/3 - SPEAK NOW
SPEECH END
TRAIN DISTANCE 1 = 412
TRAIN DISTANCE 2 = 508
TRAIN DISTANCE 3 = 377
TEMPLATE SAVED
THRESHOLD = 2076
IDLE - LISTENING FOR PASSPHRASE
SPEECH START
SPEECH END
MATCH SCORE = 903
THRESHOLD = 2076
ACCESS GRANTED
SERVO OPEN
AUDIO START
AUDIO END
SERVO CLOSED
IDLE - LISTENING FOR PASSPHRASE
```

(The numeric values above are illustrative, not measured — they show the shape
of the trace, not results from a run.)

Two lines are diagnostics rather than status:

* **`TRAIN DISTANCE n`** — how far each training utterance sits from the
  finished template. If these are large, the three recordings disagree and no
  threshold will rescue the result. Retrain.
* **`ADC OVERRUN = n`** — frames the main loop failed to collect in time. This
  should never appear; if it does, the timing analysis in
  [timers.md](timers.md) is wrong for your build and the features are missing
  samples.

## Testing without Proteus

The recognition chain does not need the simulator. `test/host_tests.cpp`
compiles `fft64.cpp`, `feature_extract.cpp`, `vad.cpp` and `recognizer.cpp`
unchanged for the host and drives them with synthetic utterances:

```sh
./test/run.sh      # the whole recognition chain, ~12000 assertions
./test/bench.sh    # per-frame cycle cost, measured under simavr
```

Debug the arithmetic there, where a failing case takes milliseconds to
reproduce, and use Proteus for what only Proteus can show you: the analogue
front end, the servo waveform and the DAC staircase.
