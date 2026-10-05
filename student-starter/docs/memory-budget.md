# Memory budget — worksheet

> **Student version.** This is a worksheet, not an answer sheet. Fill in the
> numbers from your own build; the reference implementation's totals are not
> given here on purpose.

The ATmega328P has **2 KB of SRAM**. That is the constraint that shapes this
whole project, and the first thing to check when something behaves strangely: a
stack that has grown down into your buffers produces symptoms that look like
anything except what they are.

## Rules

* **No dynamic allocation.** No `malloc`, no `new`, no `String`, no recursion.
  Everything is statically sized, so the linker can tell you the total.
* **Nothing is stored at sample rate.** 1.6 s at 8 kHz is 12800 samples — 25 KB
  as `int16_t`. Fold each frame into an accumulator and throw it away.
* **Constant tables go in `PROGMEM`.** The window and twiddle tables already do
  (`dsp_tables.h`), and so does the audio clip. Anything else you precompute
  should too.

## Measuring

```sh
pio run                                                  # totals
avr-nm --size-sort -S -td .pio/build/uno/firmware.elf    # per symbol
avr-g++ -mmcu=atmega328p -DF_CPU=16000000UL -Os -std=gnu++17 \
        -fno-exceptions -fno-rtti -Iinclude -fstack-usage -c src/*.cpp
cat *.su                                                 # per-function stack
```

`avr-nm` and `avr-g++` are in `~/.platformio/packages/toolchain-atmelavr/bin/`.

## Fill this in

### SRAM

| Item | Where | Bytes | Your value |
|---|---|---:|---|
| ADC ping-pong buffers | `adc_sampler.cpp` | 2 × 64 × 2 = 256 | |
| FFT real buffer | `feature_extract.cpp` | 64 × 2 = 128 | |
| FFT imaginary buffer | `feature_extract.cpp` | 64 × 2 = 128 | |
| Window table | `dsp_tables.h` | **0** (PROGMEM) | |
| Twiddle table | `dsp_tables.h` | **0** (PROGMEM) | |
| Segment accumulators (`RawFeatures`) | `feature_extract.cpp` | `sizeof(RawFeatures)` = ? | |
| Final feature vector | `main.cpp` | `FEATURE_COUNT` = 80 | |
| Training vectors | `main.cpp` | your choice — see below | |
| Template + threshold | `main.cpp` | `sizeof(StoredTemplate)` = ? | |
| State, flags, timestamps | everywhere | | |
| **Total (.data + .bss)** | `pio run` | | |
| **Free** | 2048 − total | | |
| **Peak stack** | `-fstack-usage`, deepest chain + worst ISR | | |
| **Margin** | free − peak stack | | |

Two of those rows are decisions, not measurements:

* **`RawFeatures`.** Work out its size from the declaration in `recognizer.h`.
  Could the band accumulators be narrower than 32 bits? What is the largest
  value one of them can actually reach with your band table, and what happens
  on the frame after it overflows?
* **Training vectors.** The obvious implementation stores all three. How many
  do you actually need to store, given that one of them was just captured into
  the vector you already have?

### Flash

| Item | Bytes | Your value |
|---|---:|---|
| `SUCCESS_AUDIO_PCM6` | see `success_audio.h` | |
| Window + twiddle tables | 128 + 128 | |
| Code | `pio run` total minus the above | |
| **Total** | | |
| **Free** | 32256 − total | |

An Arduino Uno reserves 512 B for its bootloader, which is why the ceiling is
32256 and not 32768. The audio clip costs **8000 bytes per second** of speech,
so decide how long a message you can afford before you record it.

### EEPROM

| Item | Bytes | Your value |
|---|---:|---|
| `sizeof(StoredTemplate)` | | |
| **Free** | 1024 − total | |

Confirm the size with a `static_assert` rather than by counting the fields by
hand — the whole point of the compile-time check is that it stays true when
someone adds a field.

## Targets

| Resource | Target |
|---|---|
| SRAM | comfortably under 2048 B, with several hundred bytes clear for the stack |
| Flash | under 32256 B including the audio |
| EEPROM | one record, under 1024 B |
| Frame time | well under 8.0 ms — measure with `./test/bench.sh` |

## If SRAM is tight

In roughly the order worth trying:

1. Are any constant tables still in SRAM? `PROGMEM` moves them for free.
2. Are the feature vectors wider than they need to be? Work out how much
   resolution the distance metric actually uses.
3. Are you storing all three training vectors when two would do?
4. Are the segment accumulators wider than the largest value they can hold?
5. Are any large buffers on the stack? Move them to file scope so the number is
   a fact rather than a guess about the deepest call path.

If you find yourself wanting to make a buffer smaller than one frame, stop —
the architecture is wrong somewhere upstream.
