# ADC: acquiring the voice signal

## Configuration

All register-level. `analogRead()` is banned, and not only by the assignment:
it busy-waits for the conversion to complete, which would burn 104 µs of every
125 µs sample period inside an interrupt handler.

| Register | Value | Meaning |
|---|---|---|
| `ADMUX` | `_BV(REFS0)` \| 0 | AVcc reference, channel ADC0 (PC0 / A0) |
| `ADCSRA` | `ADEN`, `ADPS2:0`=111, `ADIE` | enabled, prescaler 128, interrupt on complete |
| `ADCSRB` | 0 | auto-trigger source unused (see below) |
| `DIDR0` | `ADC0D` | digital input buffer disabled on PC0 |

**Prescaler 128** gives a 16 MHz / 128 = 125 kHz ADC clock. The datasheet wants
50–200 kHz for full 10-bit accuracy. A normal conversion takes 13 ADC clocks =
**104 µs**, which fits inside the 125 µs sample period with 21 µs to spare.

**`DIDR0`** turns off the digital input buffer on the analogue pin. A CMOS input
buffer sitting at a mid-rail voltage draws current and switches, injecting noise
into the very signal being measured.

**The first conversion takes 25 clocks, not 13** - 200 µs, longer than one
sample period - because it also initialises the analogue core. `adc_init()`
runs one throwaway conversion so the first real sample is on time. This is the
kind of detail that shows up as a single corrupted frame at the start of every
capture if you skip it.

## Triggering

Timer2's compare match is **not** one of the ATmega328P's ADC auto-trigger
sources (`ADTS` offers Timer0 compare A, Timer0 overflow, Timer1 compare B,
Timer1 overflow, Timer1 capture, free running, the analogue comparator, and
INT0 - but no Timer2). So the conversion is started in software from the
Timer2 interrupt:

```c
ISR(TIMER2_COMPA_vect) {          // 8 kHz
    if (mode == SAMPLING) adc_trigger_conversion();   // ADCSRA |= _BV(ADSC);
    ...
}

ISR(ADC_vect) {                   // ~104 us later
    int16_t sample = (int16_t)ADC - 512;
    buf[fill][index] = sample;
    ...
}
```

The jitter this introduces is the interrupt latency of the Timer2 handler,
which is a handful of cycles - well under a microsecond against a 125 µs
period, so it does not measurably affect the spectrum.

## Centring

The input is biased to 2.5 V, so silence reads about 512 on the 10-bit scale.
Every sample is stored as:

```c
int16_t centered = (int16_t)ADC - 512;
```

Signed and centred on zero is what both the FFT and the zero-crossing counter
need. A zero-crossing counter run on unsigned 0..1023 data would count nothing
at all, because the signal never crosses zero.

Range is −512 .. +511, which is why every accumulator width in the feature
extractor is derived from 512: one segment's energy is at most
1600 × 512² = 4.19 × 10⁸, and a frame's sum of |x| is at most 64 × 512 = 32768.

## Ping-pong buffering

```
        ADC_vect                          main loop
           |                                  |
   fills buf[fill] ---- 64 samples ---->  processes buf[ready]
           |                                  | 1.75 ms of FFT + features
      swaps buffers                           |
           |                                  v
      keeps filling                    adc_frame_release()
```

The interrupt does nothing but read, centre and store. All the DSP happens in
the main loop on a frame the interrupt has already finished with. The measured
per-frame cost is 1.75 ms against an 8 ms budget
(see [timers.md](timers.md)), so the main loop is never the bottleneck.

**Overrun handling.** If a buffer fills while the main loop still owns the other
one, the interrupt drops the new frame and refills the same buffer rather than
swapping. Swapping would hand the interrupt the memory the main loop is
reading, and the corruption would be silent. The dropped frames are counted and
`adc_overrun_count()` is printed after every capture, so "it never happens" is
an observation and not an assumption.

## Input circuit

```
   WAV source / microphone
            |
           C1  1 uF          AC coupling; with R1||R2 = 50k the corner is
            |                1/(2*pi*50k*1u) = 3.2 Hz, so no speech is lost
            +---- R1 100k ---- +5V
            |
            +---- R2 100k ---- GND      2.5 V bias -> ADC reads ~512 at rest
            |
           R3  4.7k
            |
            +---- C2 10 nF --- GND      anti-alias: 1/(2*pi*4.7k*10n) = 3.4 kHz
            |
           A0 (PC0)
```

* **The bias must be there.** The ATmega328P's ADC cannot read a negative
  voltage, and an audio signal swings both ways. Without the 2.5 V offset you
  would lose the entire negative half of the waveform, and the resulting
  full-wave-rectified mess would have a completely different spectrum.
* **A0 must stay within 0 V .. 5 V.** With a 2.5 V bias that allows a ±2.5 V
  peak signal. In Proteus, set the WAV/audio generator amplitude so the scope
  trace at A0 never clips against either rail. Clipping generates harmonics
  that land in the analysis bands and are not in the original recording.
* **Source impedance.** The ATmega328P datasheet recommends a source impedance
  of 10 kΩ or less so the sample-and-hold capacitor can charge in time.
  Looking back from A0 the impedance is R3 + (R1‖R2‖source) ≈ 5.3 kΩ at speech
  frequencies. Within spec.

### Known limitation: the anti-alias filter is only first-order

R3/C2 is a single pole at 3.4 kHz. At Nyquist (4 kHz) it attenuates by only
about 1.5 dB, and at 5 kHz by 3.4 dB. Anything above 4 kHz in the source folds
back into the analysis band.

This is acceptable here for two reasons: the Proteus WAV source is already band
limited by whatever produced the file, and `tools/prepare_voice_dataset.py`
low-passes at 0.45 × f<sub>s</sub> before decimating. It would not be acceptable
with a live microphone. The upgrade path is a second-order Sallen–Key stage
around an LM358, or a switched-capacitor filter like the MAX7401 that the
Cornell project used - see [cornell-comparison.md](cornell-comparison.md).

The top analysis band (2875–3500 Hz) sits on the filter's shoulder and is
attenuated by roughly 1–2 dB. That is consistent between training and testing,
so it does not bias the comparison; it does mean band 7 carries slightly less
information than the others.

## Validating in Proteus

Put oscilloscope channel A on A0. You should see:

* a DC level of 2.5 V with no input;
* the audio riding on that DC level, never touching 0 V or 5 V;
* no visible clipping on the loudest part of the utterance.

If the trace is centred on 0 V, C1 or the divider is wrong. If it is centred on
5 V or 0 V, one of R1/R2 is missing. If it clips, reduce the generator
amplitude.
