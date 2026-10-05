# PWM: driving the parking gate servo

## Why hardware PWM, not a library

`Servo.h`, `Servo.write()`, `analogWrite()` and `delay()` are all banned. The
reasons are real, not procedural:

* `analogWrite()` on pin 9 produces **490 Hz** with 8-bit resolution. A hobby
  servo expects 50 Hz and needs about 1 µs of pulse-width resolution; 8 bits of
  a 490 Hz period is 8 µs, and the frequency is wrong by a factor of ten.
* `Servo.h` would claim a timer this project has already allocated, and it
  drives the pin from an interrupt - CPU work to hold a position that the
  hardware can hold for free.
* `delay()` would make the 3-second gate-open time block the main loop, and
  during those three seconds nothing else could run.

## Timer1 configuration

Fast PWM, mode 14 (`WGM13:0` = 1110), TOP = `ICR1`, prescaler 8, non-inverting
output on OC1A.

```
tick    = 8 / 16 MHz               = 0.5 us
period  = (ICR1 + 1) * tick
        = (39999 + 1) * 0.5 us     = 20.0 ms  ->  50 Hz
OCR1A   = pulse width in 0.5 us ticks
```

| Register | Value | Meaning |
|---|---|---|
| `TCCR1A` | `COM1A1` \| `WGM11` | non-inverting OC1A, mode 14 low bits |
| `TCCR1B` | `WGM13` \| `WGM12` \| `CS11` | mode 14 high bits, prescaler 8 |
| `ICR1` | 39999 | TOP, sets the 20 ms period |
| `OCR1A` | 2000 or 4000 | pulse width |
| `DDRB` | `PB1` set | OC1A only reaches the pin if PB1 is an output |

Mode 14 is chosen over mode 15 (TOP = OCR1A) because mode 15 would consume
OCR1A for the period and leave only OCR1B for the pulse. With `ICR1` as TOP,
`OCR1A` is free and OC1A is the natural output.

## Positions

| State | `OCR1A` | Pulse width | Nominal angle |
|---|---|---|---|
| locked | 2000 | 1.000 ms | 0° |
| unlocked | 4000 | 2.000 ms | 90° |

`servo_configure(locked_ticks, unlocked_ticks)` overrides both at runtime, and
clamps to 1000 .. 5000 ticks (0.5 ms .. 2.5 ms). That clamp is not decoration: a
pulse outside that window drives a hobby servo into its mechanical end stop,
where it buzzes and draws stall current until something gives.

**Real servos will not agree with the nominal figures.** 1.0 ms / 2.0 ms is the
common convention, but a particular servo and gate arm may need 0.9 ms / 2.1 ms
to reach the actual stops, or may hit them early. Calibrate against the
hardware you have and set the endpoints with `servo_configure()`; do not assume
the datasheet numbers describe your gate.

## Why this makes the 3-second hold free

Once `ICR1` and `OCR1A` are written, the timer generates the waveform in
hardware. Holding the gate at 90° for three seconds costs **zero CPU cycles**.
That is what lets the main loop stay non-blocking while the gate is open and the
audio message is streaming out of Timer2 at the same time - `GATE_OPEN` is just
a deadline comparison against `systick_ms()`.

## Validating in Proteus

Put oscilloscope channel B on D9.

| What to check | Expected |
|---|---|
| Period | 20.0 ms (50 Hz) |
| Pulse width, locked | 1.0 ms |
| Pulse width, unlocked | 2.0 ms |
| Transition | pulse width changes between one period and the next, no glitch |

Proteus's `MOTOR-PWMSERVO` model responds to the pulse width directly, so you
should also see the horn swing on `SERVO OPEN` and return on `SERVO CLOSED`,
three seconds apart, while the UART trace confirms both.

If the period is 20 ms but the horn does not move, check that PB1 is configured
as an output - the compare unit toggles internally whether or not the pin is
driven.
