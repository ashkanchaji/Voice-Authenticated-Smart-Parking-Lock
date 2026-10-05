# Validation checklist

Work down the list. Each row is one measurement with a number attached, so
"it seems to work" never has to be the answer.

Tick the boxes as you go; the completed list is part of the deliverable.

---

## A. Power-on

| # | Check | Expected | ✓ |
|---|---|---|---|
| A1 | Virtual terminal shows the boot banner | `BOOT: VOICE PARKING LOCK` | |
| A2 | Reported sample rate | `SAMPLE RATE HZ = 8000` | |
| A3 | Reported capture length | `CAPTURE FRAMES = 200` | |
| A4 | Reported feature count | `FEATURES = 80` | |
| A5 | First run, blank EEPROM | `EEPROM INVALID` then `TRAINING REQUIRED` | |
| A6 | Servo at rest | horn at the locked position | |
| A7 | Both LEDs | off | |
| A8 | DAC output (scope C) | steady 2.5 V (code 32) | |

---

## B. ADC — acquisition

| # | Check | Expected | ✓ |
|---|---|---|---|
| B1 | Scope A, no audio playing | flat 2.5 V DC | |
| B2 | Scope A, during speech | audio riding on 2.5 V | |
| B3 | Scope A, peak excursion | never reaches 0 V or 5 V | |
| B4 | Remove `C1` and re-run | trace centred on 0 V — put it back | |
| B5 | Raise the generator amplitude until it clips | flat tops appear; note the amplitude, then back off | |
| B6 | After any capture | no `ADC OVERRUN` line in the terminal | |

**B3 matters.** A clipped input generates harmonics that land in the analysis
bands and are not in the original recording, so the features describe the
clipping rather than the voice.

---

## C. Voice activity detection

| # | Check | Expected | ✓ |
|---|---|---|---|
| C1 | During silence | terminal stays quiet, no `SPEECH START` | |
| C2 | When an utterance begins | `SPEECH START` within ~30 ms | |
| C3 | 1.6 s later | `SPEECH END` | |
| C4 | Time between C2 and C3 | 1.60 s ± 0.02 s (measure on the scope) | |
| C5 | No speech for 15 s in training | `TIMEOUT - NO SPEECH DETECTED` | |

---

## D. Training

| # | Check | Expected | ✓ |
|---|---|---|---|
| D1 | Press the button | `TRAINING SAMPLE 1/3 - SPEAK NOW` | |
| D2 | After the first utterance | prompt for sample 2/3 | |
| D3 | After the third utterance | three `TRAIN DISTANCE n = ...` lines | |
| D4 | The three distances | all well under the printed threshold | |
| D5 | | `TEMPLATE SAVED` | |
| D6 | | `THRESHOLD = ...`, between 1060 and 7956 | |
| D7 | If `WARNING: THRESHOLD CLAMPED` appears | the recordings disagree — retrain | |
| D8 | Reset the MCU | `EEPROM VALID` and the same threshold | |
| D9 | Hold the button for 2 s | `TEMPLATE ERASED`, `TRAINING REQUIRED` | |

---

## E. Recognition — the six scenarios

| # | Stimulus | Expected | ✓ |
|---|---|---|---|
| E1 | authorised speaker + correct phrase | `ACCESS GRANTED` | |
| E2 | authorised speaker + a different phrase | `ACCESS DENIED` | |
| E3 | a different speaker + the correct phrase | `ACCESS DENIED` | |
| E4 | a different speaker + a different phrase | `ACCESS DENIED` | |
| E5 | silence | no `SPEECH START`, gate stays shut | |
| E6 | broadband noise | either no trigger, or `ACCESS DENIED` | |

Record the `MATCH SCORE` for each. E1 should be clearly below the threshold and
E2–E4 clearly above; a score that lands within a few percent of the threshold
means the margin is too thin to call the result reliable.

---

## F. PWM — the gate servo

| # | Check | Expected | ✓ |
|---|---|---|---|
| F1 | Scope B period, at rest | 20.0 ms (50 Hz) | |
| F2 | Scope B pulse width, locked | 1.00 ms | |
| F3 | On `ACCESS GRANTED` | terminal prints `SERVO OPEN` | |
| F4 | Scope B pulse width, unlocked | 2.00 ms | |
| F5 | Servo horn | swings ~90° | |
| F6 | Transition | pulse width changes between periods, no runt pulse | |
| F7 | 3.0 s after the grant | `SERVO CLOSED`, pulse back to 1.00 ms | |
| F8 | Measure the open time | 3.00 s ± 0.05 s | |

---

## G. DAC — the R-2R ladder

| # | Check | Expected | ✓ |
|---|---|---|---|
| G1 | Scope C, idle | 2.5 V | |
| G2 | On `ACCESS GRANTED` | terminal prints `AUDIO START` | |
| G3 | Scope C, ramp section of the clip | a clean staircase, 0 V → 4.92 V | |
| G4 | Count the steps | 64 | |
| G5 | Step height | 78 mV ± a few mV, all equal | |
| G6 | Step duration | 125 µs | |
| G7 | Scope C, tone section | a stepped 1 kHz sine | |
| G8 | Scope D (after the filter and amplifier) | the same shape, edges rounded, larger | |
| G9 | End of clip | `AUDIO END`, scope C returns to 2.5 V with no step to 0 V | |
| G10 | UART during playback | still readable — PD0/PD1 not being clobbered | |

**G5 is the one that finds wiring errors.** Unequal steps mean a 10 k and a
20 k are swapped; a mirrored ramp means D2 and D7 are reversed; a step of
double height means one bit is stuck.

---

## H. Concurrency

| # | Check | Expected | ✓ |
|---|---|---|---|
| H1 | During the 3 s gate-open | scope B still shows 50 Hz PWM **and** scope C shows the audio staircase | |
| H2 | | the UART trace is uninterrupted | |
| H3 | | the servo does not jitter while audio plays | |

H1 is the point of the whole timer allocation: Timer1 holds the gate in
hardware while Timer2 streams the DAC, and neither needs the CPU.

---

## I. Rejection path

| # | Check | Expected | ✓ |
|---|---|---|---|
| I1 | On a mismatch | `ACCESS DENIED` | |
| I2 | Red LED | on for 1.0 s | |
| I3 | Servo | does not move | |
| I4 | DAC | stays at 2.5 V, no audio | |
| I5 | After 1 s | back to `IDLE - LISTENING FOR PASSPHRASE` | |

---

## J. Housekeeping

| # | Check | Expected | ✓ |
|---|---|---|---|
| J1 | `pio run` | 0 errors, 0 warnings | |
| J2 | Reported RAM | under 2048 B with room for the stack | |
| J3 | Reported Flash | under 32256 B | |
| J4 | `./test/run.sh` | all checks pass | |
| J5 | `./test/bench.sh` | per-frame cost well under 128000 cycles | |
| J6 | `python3 -m unittest discover tools/tests` | all pass | |
