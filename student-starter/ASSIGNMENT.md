# Assignment — Voice-Authenticated Smart Parking Lock

**سامانه قفل هوشمند صوتی پارکینگ مبتنی بر ADC، DAC و PWM**

Platform: **Arduino Uno R3 / ATmega328P**, simulated in **Proteus**.

---

## 1. What you are building

A parking gate that opens for one authorised person saying one passphrase —
«پارکینگ باز شو» — and stays shut for everyone and everything else.

The device is trained once on three recordings of that person saying that
phrase. From then on it listens, and when it hears something close enough to
what it learned it swings a servo-driven barrier open, plays a spoken
confirmation through a home-made DAC, and closes again three seconds later.

**The subject of this assignment is three peripherals**, not speech
recognition:

| | |
|---|---|
| **ADC** | acquiring the voice signal |
| **DAC** | playing back the confirmation message |
| **PWM** | controlling the gate servo |

The recogniser exists to give those three something worth doing. About sixty of
the hundred marks are on the peripheral and timing work; the DSP is worth about
thirty. Budget your time accordingly.

---

## 2. Required behaviour

```
Authorized speaker + correct phrase     => ACCEPT
Authorized speaker + wrong phrase       => REJECT
Unauthorized speaker + correct phrase   => REJECT
Unauthorized speaker + wrong phrase     => REJECT
Noise / silence                         => REJECT / remain idle
```

On ACCEPT, three things happen:

1. the servo drives the barrier to roughly 90°;
2. **at the same time**, the confirmation message plays through the R-2R DAC;
3. three seconds later the servo returns the barrier to closed.

"At the same time" is not a figure of speech. Timer1 holds the gate in
hardware while Timer2 streams the audio, and the main loop is free throughout.
If your gate waits for the audio to finish, you have not met the requirement.

On REJECT: red LED for one second, servo does not move, no audio.

**This is not speech-to-text.** The system never recovers the words. Do not
claim in your report that it does. See
[docs/recognition-algorithm.md](docs/recognition-algorithm.md).

---

## 3. Inputs and outputs

| Signal | Pin | Notes |
|---|---|---|
| Voice in | A0 / PC0 / ADC0 | biased to 2.5 V, RC anti-aliased |
| Training button | D8 / PB0 | active low, internal pull-up |
| Servo PWM out | D9 / PB1 / OC1A | 50 Hz |
| DAC out | D2..D7 / PD2..PD7 | 6-bit R-2R ladder, D2 = LSB |
| Green LED | A1 / PC1 | ACCESS GRANTED |
| Red LED | A2 / PC2 | ACCESS DENIED |
| UART | D0, D1 | 115200 8N1, debug only |

Full detail and the reasoning: [proteus/pin-map.md](proteus/pin-map.md).

---

## 4. System diagram

```
 WAV generator / microphone
        |
   2.5 V bias + RC anti-alias
        |
       A0 ---> ADC0, 10-bit, 8 kHz, interrupt-driven
        |
   ping-pong buffer, 2 x 64 samples
        |
   main loop, one frame every 8 ms
        |
        +-- frame magnitude ---> VAD ---> SPEECH_START / SPEECH_END
        |
        +-- energy, zero crossings, Hamming -> FFT64 -> 8 bands
                        |
              8 segments x 200 ms
                        |
              80 normalised features
                        |
          +-------------+-------------+
          |                           |
    training: average 3,        testing: weighted
    derive threshold,           Manhattan distance
    store in EEPROM                    |
                              distance <= threshold ?
                                 |              |
                               ACCEPT        REJECT
                                 |              |
                   Timer1 servo + Timer2 DAC   red LED
```

Timer allocation is fixed:

| Timer | Job |
|---|---|
| Timer0 | 1 ms system tick |
| Timer1 | 50 Hz servo PWM on OC1A |
| Timer2 | 8 kHz clock, shared between ADC sampling and DAC playback |

---

## 5. What you must implement

Every `TODO` in `src/` and `include/`. Find them with:

```sh
grep -rn "TODO" src/ include/
```

| Area | Files | What |
|---|---|---|
| System tick | `systick.cpp` | Timer0 CTC at 1 ms, atomic 32-bit counter |
| Timer2 ownership | `timer2.cpp` | 8 kHz CTC, mode switching, ISR dispatch |
| ADC | `adc_sampler.cpp` | register-level config, ping-pong buffering, `ADC_vect` |
| VAD | `vad.cpp` | noise floor, adaptive threshold, start/end hysteresis |
| Features | `feature_extract.cpp` | band table, streaming accumulators |
| Recognition | `recognizer.cpp` | normalisation, distance, template average, threshold |
| EEPROM | `template_store.cpp` | register-level read/write, magic + version + CRC |
| Servo | `servo.cpp` | Timer1 Fast PWM mode 14, lock/unlock, calibration |
| DAC | `dac_audio.cpp` | PORTD masking, PROGMEM streaming from the Timer2 ISR |
| State machine | `main.cpp` | all nine states, non-blocking |
| Constants | `config.h` | every threshold, weight and scale factor you choose |

**Given to you, finished:** the FFT and its tables, `success_audio.h`, the UART,
the pin map, the state enum, the dispatch loop, the LED and button plumbing, the
host test harness, the cycle benchmark, and the Proteus wiring specification.

---

## 6. Banned APIs

In the main implementation, in both the ADC/DAC/PWM path and everywhere else:

```cpp
analogRead()    analogWrite()    Servo.h    Servo.write()    tone()    delay()
```

Each ban has a reason specific to this design:

* **`analogRead()`** busy-waits for the conversion — 104 µs of every 125 µs
  sample period, inside an interrupt.
* **`analogWrite()`** on pin 9 gives 490 Hz with 8-bit resolution. A servo needs
  50 Hz and about 1 µs of pulse resolution.
* **`Servo.h`** claims a timer this project has already allocated, and drives
  the pin from an interrupt — CPU work to hold a position the hardware holds for
  free.
* **`delay()`** would block the main loop for the whole 3-second gate-open
  period, during which nothing else could run.
* **`tone()`** claims Timer2, which is the audio clock.

`Serial` is unavailable anyway: this project is built **without the Arduino
framework**, because the Arduino core claims Timer0 for `millis()`/`delay()`.
`uart.cpp` is provided as the register-level replacement, and UART debug output
is encouraged.

The `EEPROM` library would be permitted, but `template_store.cpp` is set up for
the register-level version, and the timed EEMPE/EEPE write sequence is one of
the few places on this chip where the datasheet's four-cycle window genuinely
matters. Do it by hand.

---

## 7. Required tests

### On your laptop

```sh
./test/run.sh
```

The host test suite compiles `recognizer.cpp`, `fft64.cpp`,
`feature_extract.cpp` and `vad.cpp` unchanged and checks the properties your
implementation has to satisfy. **It fails against the starter stubs — that is
the point.** Make it pass before you go near Proteus; a scaling bug takes
milliseconds to reproduce here and an afternoon in a simulator.

Keep those four files free of AVR registers. The moment one of them includes
`<avr/io.h>`, the host tests stop building and you lose your fastest debugging
loop.

```sh
./test/bench.sh        # needs: pio pkg install -g -t tool-simavr
```

Reports your per-frame DSP cost in CPU cycles against the 128 000-cycle budget.
Record the number in your report.

```sh
python3 -m unittest discover tools/tests
```

### In Proteus

Work through [proteus/validation-checklist.md](proteus/validation-checklist.md)
and submit the completed list. It covers all six recognition scenarios plus the
ADC, PWM and DAC measurements.

### Recordings

You need six WAV files. [assets/README.md](assets/README.md) specifies exactly
what to record and how; `tools/prepare_voice_dataset.py` assembles them into a
single `scenario.wav` for the Proteus audio generator.

| File | Speaker | Phrase |
|---|---|---|
| `train1.wav`, `train2.wav`, `train3.wav` | you | «پارکینگ باز شو» |
| `correct.wav` | you | «پارکینگ باز شو» |
| `wrong_phrase.wav` | you | anything else |
| `wrong_speaker.wav` | someone else | «پارکینگ باز شو» |

Record all three training utterances in one sitting, in the same room, at the
same pace. Do **not** copy one file three times: the acceptance threshold is
derived from how much the three disagree, and three identical files produce a
threshold so tight you will fail your own demo.

---

## 8. Deliverables

1. **Source**, building with `pio run` with zero errors and zero warnings.
2. **`firmware.hex`** from that build.
3. **A completed [validation checklist](proteus/validation-checklist.md).**
4. **Your Proteus project file** (`.pdsprj`) plus a screenshot of the schematic.
5. **Oscilloscope captures** of:
   * the biased audio at A0;
   * the servo PWM at D9, showing both 1 ms and 2 ms pulse widths;
   * the DAC staircase at the ladder output, before and after the filter.
6. **A UART transcript** of a complete session: boot, training, one accept, one
   reject.
7. **A completed [memory budget](docs/memory-budget.md)** with your own numbers,
   including the measured per-frame cycle count.
8. **A short report (4–6 pages)** covering:
   * your register configuration for each of the three timers and the ADC, and
     why each value;
   * your band table and why those boundaries;
   * your normalisation scheme, and specifically how it makes the system
     immune to how loudly someone speaks;
   * your distance weights and why that split;
   * your threshold formula, including what the floor and the ceiling are for;
   * your measured results for all six scenarios, with the match scores;
   * the limitations of the approach — see
     [docs/recognition-algorithm.md](docs/recognition-algorithm.md#limitations).
     A report that claims more than the system delivers loses marks.
9. **Your audio files**, or a note saying why they cannot be shared.

---

## 9. Demonstration

Fifteen minutes, live in Proteus. Be ready to:

1. **Boot from a blank EEPROM** and show `TRAINING REQUIRED`.
2. **Train** on three utterances, and read out the three training distances and
   the derived threshold. Explain what those numbers mean.
3. **Reset the MCU** and show `EEPROM VALID` with the same threshold — the
   template survived.
4. **Run all four recognition cases**, reading out each match score.
5. **On the accept**, show on one oscilloscope screen that the servo PWM and
   the DAC output are running at the same time.
6. **Measure**, live, with the scope cursors: the PWM period, both pulse
   widths, and the DAC step height.
7. **Long-press** the training button to erase the template.
8. **Answer questions**, most likely including:
   * Why can Timer2 be shared between the ADC and the DAC?
   * What happens if the main loop misses a frame deadline, and how do you know
     it has not?
   * Why does the ADC value have 512 subtracted from it?
   * Why does the DAC write mask PD0 and PD1?
   * Why is the acceptance threshold computed rather than chosen?
   * Where does your firmware's SRAM go?

---

## 10. Getting unstuck

| Symptom | Look at |
|---|---|
| Nothing prints | UART baud, and whether Timer0 is ticking at all |
| `systick_ms()` never advances | `TIMSK0`, and whether `sei()` ran |
| Timeouts fire at strange moments | the atomic read of the 32-bit tick counter |
| Servo does not move but D9 shows PWM | `PB1` is not an output |
| DAC staircase mirrored or scrambled | D2/D7 swapped — LSB and MSB reversed |
| DAC steps unequal | a 10 k and a 20 k swapped in the ladder |
| UART garbage during playback only | the DAC write is clobbering PD0/PD1 |
| VAD triggers on silence | the noise floor is being updated during speech |
| VAD never triggers | the absolute margin above the noise floor is too large |
| Everything matches, including impostors | the threshold has no ceiling, or the distance returns 0 |
| Nothing ever matches | the threshold has no floor, or the normalisation collapsed |
| Loud speakers always pass | the features are not loudness-normalised |
| `ADC OVERRUN` appears | the main loop is missing frame deadlines — run `./test/bench.sh` |
| Works on the host, fails on the AVR | check for 16-bit `int` overflow: `1600 * 512` is fine, `1600 * 512 * 2` is not |

That last row is worth reading twice. On the AVR `int` is **16 bits**, not 32.
An expression that is correct on your laptop can silently overflow on the
target, and the host tests will not catch it. Put `UL` on your constants and
cast to `uint32_t` before you multiply.
