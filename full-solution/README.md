# Full solution — reference implementation

Complete, buildable firmware for the voice-authenticated parking lock. Every
feature in the assignment is genuinely implemented: nothing returns a constant,
nothing is stubbed, and there is no "always accept" path.

## Build

```sh
pio run
```

Produces `.pio/build/uno/firmware.hex` and `firmware.elf`. Load either into the
Proteus Arduino Uno model (Program File property); the `.elf` also carries
debug symbols for source-level stepping.

Verified with PlatformIO 6.1.19 / atmelavr 5.3.0 / avr-gcc 7.3.0:

```
RAM:   [======    ]  57.8% (used 1183 bytes from 2048 bytes)
Flash: [===       ]  29.0% (used 9360 bytes from 32256 bytes)
```

Zero errors, zero warnings under `-Wall -Wextra`.

Note there is no `framework = arduino` line in `platformio.ini`. That is
deliberate: the Arduino core claims Timer0 for `millis()`/`delay()` and
installs its own `TIMER0_OVF_vect`, which collides with this project's 1 ms
system tick. Dropping it also removes about 1 KB of flash and the
`HardwareSerial` ring buffers from SRAM.

## Test

```sh
./test/run.sh      # 12220 assertions over the whole recognition chain, on the host
./test/bench.sh    # per-frame cycle count, measured under simavr
```

`test/run.sh` compiles `fft64.cpp`, `feature_extract.cpp`, `vad.cpp` and
`recognizer.cpp` unchanged for the host — those four modules touch no AVR
register — and drives the real feature extractor with synthetic utterances. A
representative run:

```
  threshold                 = 1516
  case 1 same speaker+phrase = 36     ACCEPT
  case 1b quarter volume     = 246    ACCEPT
  case 2 wrong phrase        = 6602   REJECT
  case 3 wrong speaker       = 3567   REJECT
  case 5 silence             = 7356   REJECT
  case 6 noise               = 12276  REJECT
```

These are synthetic signals; the separation is much cleaner than real speech
will give. They prove the arithmetic and the normalisation, not the accuracy.

`test/bench.sh` needs simavr (`pio pkg install -g -t tool-simavr`) and reports:

```
window + fft64       : 20908 cycles,  1306.7 us,  16% of the 8 ms frame budget
feature_process_frame: 28069 cycles,  1754.3 us,  21% of the 8 ms frame budget
frame budget         : 128000 cycles (8.0 ms)
```

Python tooling:

```sh
cd .. && python3 -m unittest discover tools/tests    # 21 tests
```

## Layout

```
full-solution/
├── platformio.ini
├── include/
│   ├── config.h            every tunable constant, with its unit and reason
│   ├── pins.h              pin map
│   ├── static_checks.h     compile-time consistency assertions
│   ├── pgm_compat.h        flash-access macros that also work on the host
│   ├── dsp_tables.h        Hamming window + FFT twiddles (generated)
│   ├── success_audio.h     6-bit PCM clip (generated; currently a placeholder)
│   └── *.h                 one header per module, documenting its contract
├── src/
│   ├── main.cpp            state machine, LEDs, button
│   ├── systick.cpp         Timer0, 1 ms tick
│   ├── timer2.cpp          Timer2 ownership and ISR dispatch
│   ├── adc_sampler.cpp     ADC config, ping-pong buffering
│   ├── servo.cpp           Timer1 PWM
│   ├── dac_audio.cpp       R-2R output, PROGMEM streaming
│   ├── template_store.cpp  EEPROM record, CRC-checked
│   ├── uart.cpp            blocking debug UART
│   ├── fft64.cpp           fixed-point FFT (adapted — see THIRD_PARTY_NOTICES)
│   ├── feature_extract.cpp streaming accumulators, band table
│   ├── vad.cpp             voice activity detection
│   └── recognizer.cpp      normalise, distance, template, threshold
├── test/                   host tests and the simavr benchmark
├── tools/                  copy of the shared Python tooling
├── docs/                   nine design documents
├── proteus/                BOM, wiring, pin map, validation checklist
└── assets/                 (empty — see assets/README.md)
```

## Where to start reading

1. [docs/architecture.md](docs/architecture.md) — the signal path and the state
   machine in one page.
2. `include/config.h` — every constant that shapes the system, with its unit
   and the reasoning behind its value.
3. [docs/timers.md](docs/timers.md) — why the frame budget holds, with measured
   numbers.
4. [docs/recognition-algorithm.md](docs/recognition-algorithm.md) — the
   recogniser, and what it cannot do.

## Known limitations

Stated plainly rather than buried:

* **The Proteus simulation has never been run.** See
  [proteus/README.md](proteus/README.md).
* **`include/success_audio.h` is a placeholder test tone**, not «ورود مجاز
  است». See [assets/README.md](assets/README.md).
* **No audio recordings are included.** They have to come from real speakers.
* **The recogniser has no time alignment** and several other structural limits
  — see [docs/recognition-algorithm.md](docs/recognition-algorithm.md#limitations).
* **The input anti-alias filter is first-order.** Adequate with a band-limited
  WAV source in simulation; not with a live microphone. See
  [docs/adc.md](docs/adc.md).
