# Grading rubric — Voice-Authenticated Smart Parking Lock

100 marks total. The weighting follows the point of the assignment: **ADC, DAC,
PWM and the timer architecture around them are 51 marks**; the whole DSP chain
is 32. A brilliant recogniser attached to a badly configured ADC scores worse
than a mediocre recogniser attached to a well-engineered one.

| # | Area | Marks |
|---|---|---:|
| 1 | ADC configuration + sampling | 15 |
| 2 | Timer / interrupt architecture | 12 |
| 3 | PWM + servo | 12 |
| 4 | R-2R DAC + playback | 12 |
| 5 | Feature extraction | 12 |
| 6 | Training + matching + threshold | 12 |
| 7 | Voice activity detection | 8 |
| 8 | Proteus validation + explanation | 7 |
| 9 | EEPROM persistence | 5 |
| 10 | State machine | 5 |
| | **Total** | **100** |

---

## 1. ADC configuration and sampling — 15

| | Marks |
|---|---:|
| `ADMUX`/`ADCSRA` configured at register level: AVcc reference, ADC0, prescaler chosen so the ADC clock is inside the datasheet's 50–200 kHz window, and the choice justified | 4 |
| Conversion started from the Timer2 interrupt and completed in `ADC_vect`; no polling, no `analogRead()` anywhere | 3 |
| Samples correctly centred (`ADC − 512`) and stored as signed | 2 |
| Ping-pong double buffering, with the ADC interrupt doing nothing but read/centre/store | 3 |
| Overrun case handled safely — the interrupt never writes the buffer the main loop is reading — and counted so it can be shown to be zero | 2 |
| Digital input buffer disabled on the analogue pin, and why | 1 |

*Deductions:* `analogRead()` anywhere in the acquisition path scores 0 for this
section. DSP inside `ADC_vect` loses 3.

---

## 2. Timer / interrupt architecture — 12

| | Marks |
|---|---:|
| Timer0 CTC at exactly 1 ms; correct prescaler and OCR value, derived not guessed | 3 |
| The 32-bit tick counter is `volatile` **and** read atomically, with an explanation of what breaks otherwise | 3 |
| Timer2 correctly configured for 8 kHz, with the right prescaler bits for *Timer2* (its encoding differs from Timer0/Timer1) | 2 |
| Timer2 mode switching between sampling and playback is clean: one owner, one ISR, explicit dispatch | 2 |
| All ISRs short; no blocking, no printing, no DSP inside any of them | 1 |
| Per-frame cost measured with `./test/bench.sh` and reported against the 8 ms budget | 1 |

*Deductions:* any use of `delay()` in the main path loses 4. A blocking wait
inside an ISR loses 4.

---

## 3. PWM + servo — 12

| | Marks |
|---|---:|
| Timer1 Fast PWM mode 14 with `ICR1` as TOP, correct prescaler, 50 Hz verified on the scope | 4 |
| Pulse widths correct and measured: 1.00 ms locked, 2.00 ms unlocked | 3 |
| Non-blocking 3-second gate-open, driven from the system tick | 2 |
| Endpoints configurable and clamped to a safe range, with a word about why a real servo needs calibrating | 2 |
| No `Servo.h`, no `Servo.write()`, no `analogWrite()` | 1 |

*Deductions:* using any servo library scores 0 for this section. A blocking
3-second wait loses 4.

---

## 4. R-2R DAC + playback — 12

| | Marks |
|---|---:|
| Ladder correctly wired, 6 bits on PD2..PD7 with D2 as LSB; staircase verified on the scope with 64 equal steps | 4 |
| PORTD write masks PD0/PD1 so the UART survives playback, and this is demonstrated | 2 |
| Playback is interrupt-driven from Timer2, streaming from `PROGMEM` with no SRAM sample buffer | 3 |
| DAC parked at mid-scale when idle and at end of clip, with the reason | 1 |
| Servo and audio genuinely concurrent — shown on one scope screen | 2 |

*Deductions:* copying the clip into an SRAM buffer loses 3. Clobbering the UART
pins loses 2.

---

## 5. Feature extraction — 12

| | Marks |
|---|---:|
| Streaming: nothing stored at sample rate, and the memory budget shows it | 3 |
| Band table covers the speech range sensibly, excludes DC and mains hum, stays under the anti-alias corner, and the boundaries are justified | 3 |
| Short-time energy and zero-crossing count correct, including carrying the sign across frame boundaries | 2 |
| Accumulator widths correct and argued from the worst-case value, not chosen by feel | 2 |
| Normalisation is loudness-invariant, handles silence without dividing by zero, and uses the full 0..255 range | 2 |

*Deductions:* a system that matches on volume — where speaking louder changes
the feature vector — loses 4 here and will also fail scenario 3.

---

## 6. Training + matching + threshold — 12

| | Marks |
|---|---:|
| Three utterances averaged into a template without overflow, rounding to nearest | 3 |
| Weighted Manhattan distance implemented correctly; weights chosen deliberately and defended | 3 |
| Threshold **derived from the training spread**, not a magic number | 3 |
| Floor and ceiling present, with an explanation of what each prevents | 2 |
| The three training distances printed as a diagnostic, and their meaning explained | 1 |

*Deductions:* a hard-coded threshold constant loses 3. Anything resembling
`return true;` in the match path scores 0 for this section.

---

## 7. Voice activity detection — 8

| | Marks |
|---|---:|
| Noise floor estimated adaptively, and only updated when it should be | 3 |
| Speech threshold derived from the noise floor **plus** an absolute margin, with an explanation of why the multiplier alone is not enough | 2 |
| Hysteresis on both edges: several frames to start, a longer hangover to end | 2 |
| Does not trigger on a single loud frame, and does not end on a single quiet one — demonstrated | 1 |

---

## 8. Proteus validation + explanation — 7

| | Marks |
|---|---:|
| Circuit built and matching the specification; schematic screenshot submitted | 2 |
| Completed validation checklist with real measured numbers, not ticks alone | 2 |
| Oscilloscope captures for ADC, PWM and DAC, with the measured values annotated | 2 |
| All six recognition scenarios demonstrated with their match scores | 1 |

---

## 9. EEPROM persistence — 5

| | Marks |
|---|---:|
| Register-level read and write, with the timed EEMPE/EEPE sequence protected and the reason given | 2 |
| Record validated by magic, version **and** CRC; a blank or stale EEPROM is rejected cleanly | 2 |
| Template survives a reset; long press erases it and forces retraining | 1 |

---

## 10. State machine — 5

| | Marks |
|---|---:|
| All nine states implemented, transitions matching the specification | 2 |
| Fully non-blocking; every duration is a deadline against the system tick | 2 |
| UART trace complete and matching the required strings | 1 |

---

## Cross-cutting deductions

These apply on top of the section marks.

| | Marks |
|---|---:|
| Any banned API in the main implementation (`analogRead`, `analogWrite`, `Servo.h`, `tone`, `delay`) | −5 each |
| Dynamic allocation, `new`, `malloc` or `String` | −5 |
| Build produces warnings under `-Wall -Wextra` | −3 |
| Magic numbers in `.cpp` files instead of named constants in `config.h` | −3 |
| Shared ISR state not `volatile` | −3 |
| Multi-byte shared variable read without atomicity | −3 |
| Report claims capabilities the system does not have (e.g. "recognises Persian speech") | −5 |

## Cross-cutting bonuses

| | Marks |
|---|---:|
| Compile-time assertions covering your own constants, not just the provided ones | +2 |
| Additional host tests for behaviour you implemented | +2 |
| Measured per-frame cycle count with an analysis of where the time goes | +2 |
| A genuine improvement to the recogniser, with measurements showing it helps | +3 |

Bonuses cannot take the total above 100.

---

## What "works" means

A submission that builds, passes its own host tests, and demonstrates all six
scenarios in Proteus with measured oscilloscope traces is a strong pass. The
marks above the pass line are for **understanding**: being able to say why the
prescaler is 128 and not 64, why the tick counter needs an atomic read, why the
threshold is computed rather than chosen, and where every byte of SRAM went.

A submission that recognises speech perfectly but cannot explain its ADC
configuration has missed the assignment.
