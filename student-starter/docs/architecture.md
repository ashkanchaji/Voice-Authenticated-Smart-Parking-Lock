# System architecture

> **Student version.** The architecture below is fixed - build this system,
> not a different one. What each module has to *do* is here and in the headers;
> how it does it is the assignment. See [../ASSIGNMENT.md](../ASSIGNMENT.md).

## What the system does

The device learns one person saying one phrase, and opens a parking gate when it
hears that person say that phrase again. It is not speech-to-text: it never
recovers the words. It compares the *acoustic shape* of an utterance against a
stored template. See [recognition-algorithm.md](recognition-algorithm.md) for
what that means and what it cannot do.

The educational point is the three peripherals, not the speech recognition:

| Peripheral | Role in this project |
|---|---|
| **ADC** | acquires the voice signal at 8 kHz, register-level, interrupt-driven |
| **PWM** | drives the gate servo at 50 Hz from Timer1 hardware PWM |
| **DAC** | plays the spoken confirmation through a 6-bit R-2R ladder |

The recogniser exists to give those three something worth doing.

## Signal path

```
 WAV / microphone
        |
   bias to 2.5 V, RC anti-alias           (see docs/adc.md)
        |
       A0  ---> ADC0, 10-bit, AVcc ref
        |
   ADC_vect: store (ADC - 512)            <- interrupt, ~30 cycles
        |
   ping-pong buffer, 2 x 64 samples
        |
   main loop, one frame every 8 ms:
        |
        +-- sum |x|            ------> VAD  ---> SPEECH_START / SPEECH_END
        |
        +-- sum x^2            ------\
        +-- zero crossings     -------+--> segment accumulators (8 segments)
        +-- Hamming -> FFT64 -> bands /
                                        |
                                  normalise to 80 x uint8
                                        |
                        +---------------+---------------+
                        |                               |
                  training: average 3            testing: weighted
                  into a template,               Manhattan distance
                  derive threshold,              against the template
                  store in EEPROM                        |
                                                  distance <= threshold ?
                                                    |             |
                                                  ACCEPT        REJECT
                                                    |             |
                                        servo unlock + audio    red LED
```

## Timer ownership

Every timer has exactly one owner. Nothing else touches its registers.

| Timer | Owner | Mode | Job |
|---|---|---|---|
| Timer0 | `systick.cpp` | CTC, /64, OCR0A=249 | 1 ms tick, `system_ms` |
| Timer1 | `servo.cpp` | Fast PWM 14, /8, ICR1=39999 | 50 Hz servo PWM on OC1A |
| Timer2 | `timer2.cpp` | CTC, /8, OCR2A=249 | 8 kHz clock, shared ADC / DAC |

Timer2 is shared because sampling and playback are mutually exclusive by
design: the gate only speaks after it has finished listening. Both jobs need
exactly the same 8 kHz tick, so only the interrupt body differs. There can be
one `TIMER2_COMPA_vect` in a program, so `timer2.cpp` owns it and dispatches on
a mode variable. See [timers.md](timers.md).

This is also why the firmware is built **without** the Arduino framework. The
Arduino core claims Timer0 for `millis()`/`delay()` and installs its own
`TIMER0_OVF_vect`, which collides head-on with the requirement that Timer0 be
this project's 1 ms tick.

## Concurrency model

There are exactly three interrupt sources and one main loop.

* `TIMER0_COMPA_vect` - increments a 32-bit counter. Nothing else.
* `TIMER2_COMPA_vect` - starts an ADC conversion, or writes one DAC sample.
* `ADC_vect` - reads one sample, centres it, stores it, and every 64th time
  publishes a frame.

No DSP may happen in any interrupt. All of it happens in the main loop, on
frames the interrupt has already finished with. That is what keeps the 8 ms
frame deadline achievable - measure your own per-frame cost with
`./test/bench.sh` (see [timers.md](timers.md)).

Shared state between the interrupts and the main loop is `volatile`, and the
one multi-byte shared variable that can be read mid-update - the 32-bit
millisecond counter - is read inside `ATOMIC_BLOCK`. A 32-bit read is four
instructions on an 8-bit core; without the guard, a tick landing between two of
them returns a value that never existed.

## State machine

```
                    +-----------+
                    |   BOOT    |
                    +-----+-----+
             EEPROM valid |  EEPROM invalid
              +-----------+-----------+
              v                       v
         +---------+         +----------------+
         |  IDLE   |<--+     | TRAINING_WAIT  |<---+
         +----+----+   |     +--------+-------+    |
   SPEECH_START |      |       button |            |
              v |      |              v            |
       +-----------+   |     +-----------------+   | more samples needed
       | LISTENING |   |     | TRAINING_LISTEN +---+
       +-----+-----+   |     +--------+--------+
  1.6 s done |         |              | 3rd sample: build template,
             v         |              | derive threshold, save to EEPROM
      +------------+   |              v
      | PROCESSING |   +--------------+
      +--+------+--+   |
   accept |      | reject
          v      v
  +----------+  +----------+
  | ACCEPTED |  | REJECTED |
  +----+-----+  +----+-----+
       |             | 1 s
       v             |
 +-----------+       |
 | GATE_OPEN |       |
 +-----+-----+       |
       | 3 s         |
       +------+------+
              |
              v  (back to IDLE)
```

A long press on the training button, from any state, erases the stored template
and returns to `TRAINING_WAIT`.

Nothing in the loop blocks. Every duration is a deadline compared against
`systick_ms()`. The one exception is the EEPROM write at the end of training,
which is inherently 3.4 ms per changed byte in hardware; it happens once, while
nothing else is running, and the tick interrupt keeps running through it.

## Module map

| File | Responsibility | Touches AVR registers? |
|---|---|---|
| `main.cpp` | state machine, LEDs, button | yes (GPIO) |
| `systick.cpp` | Timer0, 1 ms tick | yes |
| `timer2.cpp` | Timer2 ownership and ISR dispatch | yes |
| `adc_sampler.cpp` | ADC config, ping-pong buffering | yes |
| `servo.cpp` | Timer1 PWM, gate position | yes |
| `dac_audio.cpp` | R-2R output, PROGMEM streaming | yes |
| `template_store.cpp` | EEPROM record, CRC-checked | yes |
| `uart.cpp` | blocking debug UART | yes |
| `fft64.cpp` | fixed-point FFT, window, magnitude | **no** |
| `feature_extract.cpp` | streaming accumulators, band table | **no** |
| `vad.cpp` | voice activity detection | **no** |
| `recognizer.cpp` | normalise, distance, template, threshold | **no** |

The bottom four compile unchanged for the host, which is how
`test/host_tests.cpp` exercises the entire recognition chain on a laptop. **Keep
peripheral code out of them** - the moment one of them includes `<avr/io.h>`
the host tests stop building and you lose your fastest debugging loop.
