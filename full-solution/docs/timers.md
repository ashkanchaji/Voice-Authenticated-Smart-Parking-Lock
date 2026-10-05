# Timers and interrupt budget

## Ownership

Exactly one module configures each timer. No other file touches its registers.

| Timer | Owner | Mode | Prescaler | TOP | Rate |
|---|---|---|---|---|---|
| Timer0 | `systick.cpp` | CTC | 64 | OCR0A = 249 | 1000 Hz |
| Timer1 | `servo.cpp` | Fast PWM 14 | 8 | ICR1 = 39999 | 50 Hz |
| Timer2 | `timer2.cpp` | CTC | 8 | OCR2A = 249 | 8000 Hz |

Each of those three relationships is checked at compile time in
`include/static_checks.h`, so changing a prescaler without changing the compare
value fails the build instead of silently shifting the sample rate by 7%.

### Timer0 — 1 ms system tick

```
16 MHz / (64 * (249 + 1)) = 1000 Hz
```

The ISR does one thing: `++s_ms`. Everything with a duration in this firmware -
the 3 s gate-open, the 1 s reject indication, the 15 s listen timeout, the
button debounce and long press - is a comparison against this counter, which is
why `delay()` never appears.

`systick_ms()` reads the counter inside `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`. A
32-bit read is four instructions on an 8-bit core; a tick landing between two of
them returns a value that never existed - a low half from before the increment
and a high half from after. It bites hardest at every 256 ms boundary, where
the low byte wraps.

### Timer1 — servo PWM

Configured once, then left alone. The waveform is generated in hardware, so
holding the gate open costs no CPU. See [pwm-servo.md](pwm-servo.md).

### Timer2 — the shared 8 kHz audio clock

Timer2 has two jobs that never overlap:

| Mode | Interrupt body | When |
|---|---|---|
| `SAMPLING` | `adc_trigger_conversion()` | IDLE, LISTENING, TRAINING_LISTEN |
| `PLAYBACK` | `dac_audio_tick()` | ACCEPTED, GATE_OPEN |
| `OFF` | — | everything else |

Sharing is safe because the gate only speaks after it has finished listening.
Both jobs need exactly the same 8 kHz tick, so only the interrupt body differs.
There can be one `TIMER2_COMPA_vect` in a program, so `timer2.cpp` owns it and
dispatches on the mode.

Note that Timer2's prescaler encoding is **not** the same as Timer0's and
Timer1's: Timer2 has /32 and /128 options the others lack, so `CS2[2:0] = 010`
selects /8 where `CS0[2:0] = 011` would on Timer0. Copying the bits across is a
classic way to end up sampling at the wrong rate.

## Interrupt cost

| Vector | Rate | Work | Approx. cycles |
|---|---|---|---|
| `TIMER0_COMPA_vect` | 1 kHz | increment a 32-bit counter | ~30 |
| `TIMER2_COMPA_vect` | 8 kHz | one branch, one register write | ~30 |
| `ADC_vect` | 8 kHz | read, subtract 512, store, maybe swap | ~45 |

Total interrupt load at 16 MHz:

```
(1000 * 30 + 8000 * 30 + 8000 * 45) / 16e6 = 0.4%
```

Interrupt overhead is negligible. The entire real-time question is whether the
main loop finishes one frame before the next arrives.

## Frame budget — measured, not estimated

One frame is 64 samples at 8 kHz = **8.0 ms = 128 000 CPU cycles**.

`test/bench.sh` builds a benchmark firmware that runs the real
`feature_process_frame()` with Timer1 free-running at the CPU clock (prescaler
1, one tick per cycle) and runs it under simavr. Measured on this codebase:

```
window + fft64       : 20908 cycles,  1306.7 us,  16% of the 8 ms frame budget
feature_process_frame: 28069 cycles,  1754.3 us,  21% of the 8 ms frame budget
             (x25 avg): 28072 cycles,  1754.5 us,  21% of the 8 ms frame budget
frame budget         : 128000 cycles (8.0 ms)
```

So the DSP uses **22% of the frame period** and there is a 4.5× margin. The
breakdown:

| Stage | Cycles | Share |
|---|---|---|
| Hamming window + pre-scale | ~1 900 | 7% |
| 64-point FFT (6 stages, 192 butterflies) | ~19 000 | 68% |
| band magnitudes (27 bins) | ~5 400 | 19% |
| energy + zero crossings (64 samples) | ~1 800 | 6% |

Rerun `./test/bench.sh` after any change to the FFT or the extractor. A claim
about real-time behaviour that has not been measured since the code changed is
not a claim.

## What must not go in the frame

Two things can blow the budget:

**UART printing.** At 115200 baud one byte takes 87 µs, so a 40-character line
takes 3.5 ms. Added to 1.75 ms of DSP that is 5.3 ms of the 8 ms budget - still
inside, but only just, and two lines would not be. The state machine therefore
prints only at state transitions. `SPEECH START` (14 bytes, 1.2 ms) is the only
print that happens anywhere near the capture loop.

**EEPROM writes.** 3.4 ms per byte, in hardware. The 90-byte record takes up to
306 ms. This happens exactly once, at the end of training, while sampling is
stopped. `template_save_to_eeprom()` skips bytes that already hold the right
value, which usually cuts it well below that.

## Proving it never overruns

The ADC interrupt counts every frame it had to drop because the main loop still
owned the other buffer, and the firmware prints `ADC OVERRUN = n` after every
capture where n is non-zero. If you ever see that line, the budget above is
wrong for your build and the features are missing samples.
