# Memory budget

Every number below is either read out of the linked ELF or produced by a tool
that ran on this codebase. Nothing here is an estimate unless it says so.

Reproduce with:

```sh
cd full-solution && pio run          # prints RAM and Flash totals
avr-nm --size-sort -S -td .pio/build/uno/firmware.elf   # per-symbol
```

## Totals

| | Used | Available | Free |
|---|---|---|---|
| **SRAM** | **1 183 B (57.8%)** | 2 048 B | 865 B |
| **Flash** | **9 360 B (29.0%)** | 32 256 B | 22 896 B |
| **EEPROM** | **90 B (8.8%)** | 1 024 B | 934 B |

Flash "available" is 32 256 B rather than 32 768 B because an Arduino Uno
reserves 512 B for its bootloader. Proteus loads the hex directly and does not
need the bootloader, so the real ceiling in simulation is slightly higher; the
smaller figure is used so the same binary can also be flashed to a real board.

## SRAM, item by item

`.data` is empty and `.bss` is 1 183 B. Every allocation is static; there is no
`malloc`, no `new`, no `String` and no recursion anywhere in the firmware.

| Bytes | Symbol | What it is |
|---:|---|---|
| 304 | `s_raw` | `RawFeatures`: 8 segment energies (u32), 8 ZCR counts (u16), 8×8 band accumulators (u32) |
| 256 | `s_buf` | ADC ping-pong, 2 × 64 × `int16_t` |
| 160 | `g_train` | training vectors 1 and 2 (the third is `g_feature`) |
| 128 | `s_fr` | FFT real buffer, 64 × `int16_t` |
| 128 | `s_fi` | FFT imaginary buffer, 64 × `int16_t` |
| 90 | `g_template` | live copy of the EEPROM record |
| 80 | `g_feature` | the current utterance's normalised features |
| 37 | — | 24 state, index, flag and timestamp variables, 1–4 B each |
| **1 183** | | **total** |

Deliberate choices behind those numbers:

* **The 12 800 samples of an utterance are never stored.** At 2 bytes each that
  would be 25 KB. Frames are folded into `s_raw` and discarded, which is why the
  extractor is written as a streaming accumulator rather than a buffer-then-
  process pipeline.
* **Features are `uint8_t`, not `int16_t`.** 8 bits per feature halves the
  vector, the template and the training store - 165 bytes saved - and the
  measurement noise between two recordings of the same phrase is far larger
  than 1/255 of full scale.
* **Only two training vectors are stored.** The third is the freshly captured
  `g_feature`, which has to exist anyway. Saves 80 bytes over `[3][80]`.
* **The window and twiddle tables cost zero SRAM.** They are `PROGMEM`
  (256 B of flash between them). The Cornell original builds its sine table at
  boot with floating-point `sin()`, which would cost 128 B of SRAM here plus
  libm in flash.
* **The FFT buffers are file-scope, not stack.** So the budget is a fact rather
  than a guess about the deepest call path.

## Stack

865 B are free. Worst-case usage, from `avr-g++ -fstack-usage`:

| Frame | Bytes |
|---|---:|
| `vpl_normalize()` | 48 |
| `fft64()` | 31 |
| `uart_put_u32()` | 22 |
| `TIMER2_COMPA_vect` | 17 |
| `feature_process_frame()` | 15 |
| `ADC_vect` | 12 |
| `vpl_isqrt()`, `vpl_distance()`, `main()` | 10 each |

The deepest main-loop chain is
`main → on_processing → feature_finalize → vpl_normalize → vpl_isqrt`
at about 80 B including return addresses. The worst interrupt that can land on
top of it is `TIMER2_COMPA_vect` at 17 B plus its saved context, roughly 25 B.
Interrupts do not nest - the AVR clears the global interrupt enable on entry and
nothing in this firmware re-enables it inside a handler.

**Peak stack is therefore about 105 B against 865 B free: an 8× margin.**

Reproduce with:

```sh
avr-g++ -mmcu=atmega328p -DF_CPU=16000000UL -Os -std=gnu++17 \
        -fno-exceptions -fno-rtti -Iinclude -fstack-usage -c src/*.cpp
cat *.su
```

## Flash, largest items

| Bytes | Symbol |
|---:|---|
| 2 400 | `SUCCESS_AUDIO_PCM6` (0.30 s placeholder clip) |
| 1 928 | `main` (state machine, inlined helpers, UART strings) |
| 1 102 | `feature_process_frame` |
| 860 | `vpl_normalize` |
| 374 | `pump_frames` |
| 170 | `ADC_vect` |
| 162 | `TIMER2_COMPA_vect` |
| 160 | `uart_put_u32` |
| 128 | `SINE_Q15` |
| 128 | `HAMMING_Q15` |
| 114 | `vpl_distance` |

The audio clip is a quarter of the total and scales at **8 000 bytes per second
of speech**. With the firmware at 9 360 B, the longest message that fits is
about 2.7 s; `tools/wav_to_6bit_header.py` caps at 1.8 s by default so an
oversized WAV fails with a clear message instead of a link error.

Two decisions saved most of the rest:

* **No Arduino framework.** The core costs roughly 1 KB of flash and puts the
  two 64-byte `HardwareSerial` ring buffers plus its object in SRAM. It also
  claims Timer0, which this project needs.
* **No `printf`.** avr-libc's `printf` family is around 1.5 KB of flash and
  brings its own SRAM overhead. `uart_put_u32()` is 160 B and is all this
  firmware needs.

## EEPROM

`sizeof(StoredTemplate)` is 90 B, verified by a `static_assert` in the AVR
build. There is no padding: the AVR has no alignment requirement, so the fields
pack tightly.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | `magic` = 0xA5C3 |
| 2 | 1 | `version` = 1 |
| 3 | 1 | `reserved` = 0 |
| 4 | 80 | `features[80]` |
| 84 | 4 | `threshold` |
| 88 | 2 | `checksum` (CRC-16/CCITT-FALSE over bytes 0..87) |

934 bytes remain free, which is room for ten more templates if the project were
ever extended to multiple authorised speakers.

Writes take 3.4 ms per byte in hardware. `template_save_to_eeprom()` reads each
byte first and only writes the ones that differ, which saves both time and
endurance - an EEPROM cell is rated for about 100 000 writes.

## Headroom summary

| Resource | Margin |
|---|---|
| SRAM | 865 B free, ~105 B of that needed for stack |
| Flash | 22 896 B free, enough for another 2.7 s of audio |
| EEPROM | 934 B free |
| Frame time | 1.75 ms used of 8.0 ms (see [timers.md](timers.md)) |

The tightest of the four is SRAM, and it still has 42% spare after the stack.
