# Timers and interrupt budget

> **Student version.** This document states what the system has to do and the
> constraints it has to satisfy. It deliberately does not give you the register
> values or the design constants - working those out from the ATmega328P
> datasheet is the assignment. See [../ASSIGNMENT.md](../ASSIGNMENT.md).

## Ownership

Exactly one module configures each timer. No other file touches its registers.

| Timer | Owner | Mode | Prescaler | TOP | Rate |
|---|---|---|---|---|---|
| Timer0 | `systick.cpp` | CTC | 64 | OCR0A = 249 | 1000 Hz |
| Timer1 | `servo.cpp` | Fast PWM 14 | 8 | ICR1 = 39999 | 50 Hz |
| Timer2 | `timer2.cpp` | CTC | 8 | OCR2A = 249 | 8000 Hz |

Each of those three relationships is checked at compile time in
`include/static_checks.h`, so changing a prescaler without changing the compare
value fails the build instead of silently shifting the sample rate by 7%. Add
the same kind of check for every constant you introduce.

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

| Vector | Rate | Work it should do |
|---|---|---|
| `TIMER0_COMPA_vect` | 1 kHz | increment a 32-bit counter - nothing else |
| `TIMER2_COMPA_vect` | 8 kHz | one branch, one register write |
| `ADC_vect` | 8 kHz | read, centre, store, occasionally publish a frame |

Each should be a few tens of cycles, so the total interrupt load at 16 MHz is
well under 1%. Work the number out for your own handlers: if any of them is
more than about 100 cycles, something in it belongs in the main loop instead.

Interrupt overhead is then negligible, and the entire real-time question is
whether the main loop finishes one frame before the next arrives.

## Frame budget - measure it, do not estimate it

One frame is 64 samples at 8 kHz = **8.0 ms = 128 000 CPU cycles**. Everything
your main loop does per frame - window, FFT, band magnitudes, energy, zero
crossings - has to fit inside that, with margin.

`test/bench.sh` builds a benchmark firmware that runs your real
`feature_process_frame()` with Timer1 free-running at the CPU clock (prescaler
1, so one tick is one cycle) and runs it under simavr:

```sh
pio pkg install -g -t tool-simavr
./test/bench.sh
```

It reports cycles, microseconds and the percentage of the frame budget. Record
those numbers in your report, and rerun it after any change to the extractor.
**A claim about real-time behaviour that has not been measured since the code
changed is not a claim.**

If it does not fit, the FFT is the first place to look: 6 stages of 32
butterflies is 192 butterflies, each with four fixed-point multiplies. But
measure before optimising - the answer is often somewhere else entirely.

## What must not go in the frame

Two things can blow the budget:

**UART printing.** At 115200 baud one byte takes 87 µs, so a 40-character line
takes 3.5 ms - a large fraction of the 8 ms budget on its own, before any DSP.
Print only at state transitions, never inside the capture loop.

**EEPROM writes.** 3.4 ms per byte, in hardware. Work out what the whole 90-byte
record costs, and make sure it only ever happens while sampling is stopped.

## Proving it never overruns

Have the ADC interrupt count every frame it had to drop because the main loop
still owned the other buffer, and print the count after every capture. If that
number is ever non-zero, your frame budget does not hold and the features are
being computed from incomplete data. Demonstrating that it stays at zero is
part of the deliverable.
