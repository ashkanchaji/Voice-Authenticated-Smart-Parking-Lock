# DAC: 6-bit R-2R ladder

The Arduino Uno has no DAC. A resistor ladder on six GPIO pins is the cheapest
way to get one, and it makes the conversion visible: you can watch the
staircase on an oscilloscope and count the steps.

## Ladder

Six bits on PD2 (LSB) .. PD7 (MSB), R = 10 kΩ, 2R = 20 kΩ.

```
  GND
   |
  20k        D2        D3        D4        D5        D6        D7
   |        (LSB)                                             (MSB)
   |         |         |         |         |         |         |
   |        20k       20k       20k       20k       20k       20k
   |         |         |         |         |         |         |
   +---N0----+---10k---N1---10k--N2---10k--N3---10k--N4---10k--N5 ---> Vout
```

* Each bit drives its own 20 kΩ into a node.
* Adjacent nodes are joined by 10 kΩ.
* The LSB end is terminated with 20 kΩ to ground.
* The output is taken at N5, the MSB node.

Looking into N5 the ladder presents a **constant 10 kΩ** regardless of the code,
which is the property that makes an R-2R ladder linear.

```
Vout = Vcc * code / 64        code = 0..63
     = 5 V  * code / 64
```

| Code | Vout | Note |
|---|---|---|
| 0 | 0.000 V | negative full scale |
| 32 | 2.500 V | silence / idle |
| 63 | 4.922 V | positive full scale |

One LSB is 5 V / 64 = **78.1 mV**.

## Writing a sample

```c
PORTD = (PORTD & DAC_KEEP_MASK) | ((code & 0x3F) << DAC_SHIFT);
//               ^ 0x03: PD0/PD1        ^ 2
```

One read-modify-write. `DAC_KEEP_MASK` preserves PD0 and PD1, which are RXD and
TXD. Clobbering them mid-playback corrupts the debug output and, worse, changes
TXD's idle level - a receiver would see it as a framing error or a break
condition.

`DDRD |= 0xFC` in `dac_init()` makes PD2..PD7 outputs and leaves PD0/PD1 alone
for the same reason.

## Streaming from flash

`success_audio.h` holds one 0..63 code per byte in `PROGMEM`. The Timer2
interrupt reads the next byte with `pgm_read_byte()` and writes the port:

```c
void dac_audio_tick(void) {          // called at 8 kHz from TIMER2_COMPA_vect
    if (s_pos >= SUCCESS_AUDIO_LEN) { dac_write6(32); timer2_stop_isr(); return; }
    dac_write6(pgm_read_byte(&SUCCESS_AUDIO_PCM6[s_pos++]));
}
```

Nothing is buffered in SRAM. A 0.3 s clip costs 2400 bytes of flash and zero
bytes of RAM; the same clip in SRAM would not fit in the chip.

The interrupt retires the timer itself when it reaches the last sample, so the
main loop never has to poll for the end of playback - it just asks
`audio_is_playing()` when it wants to print `AUDIO END`.

**Silence is mid-scale, not zero.** The ladder is unipolar. `dac_init()` and the
end of playback both park the DAC at code 32, because dropping to code 0 would
put a 2.5 V step through the coupling capacitor and produce an audible click
through the amplifier.

## Output stage

```
  N5 (10k source impedance)
   |
   +---- C3 4.7 nF ---- GND       reconstruction filter, see below
   |
  C4  10 uF                       AC coupling; removes the 2.5 V DC offset
   |
  RV1 10k log pot                 volume
   |
  LM386 pin 3 (+IN)               gain 20 by default (pins 1 and 8 open)
   |
  LM386 pin 5 (OUT)
   |
  C5 220 uF
   |
  8 ohm speaker ---- GND
```

**The reconstruction filter is free.** The ladder's own 10 kΩ output impedance
plus C3 = 4.7 nF gives a corner at

```
f = 1 / (2*pi*10k*4.7n) = 3.39 kHz
```

which sits between the top of the speech band (3.5 kHz) and the 8 kHz sample
rate, so it removes most of the staircase's high-frequency image without
needing an active stage.

**The volume pot loads the ladder.** A 10 kΩ pot across a 10 kΩ source halves
the signal at maximum. That is fine - the LM386's gain of 20 has plenty of
headroom - but it is worth knowing before you wonder where half the amplitude
went. If you want the full swing, use a 100 kΩ pot instead.

## Flash budget

One byte per sample at 8 kHz is **8000 bytes per second of audio**.

| Clip | Samples | Flash |
|---|---|---|
| placeholder test tone (0.30 s) | 2400 | 2400 B |
| a typical spoken phrase (1.5 s) | 12000 | 12000 B |
| `--max-seconds` default (1.8 s) | 14400 | 14400 B |

The firmware itself is about 9.4 KB, so a 1.8 s message brings the total to
roughly 24 KB of the 31.5 KB available to an application on a Uno. The
`--max-seconds` guard in `tools/wav_to_6bit_header.py` exists so that an
oversized WAV fails with a clear message instead of an obscure link error.

Packing four 6-bit samples into three bytes would save 25% of that, at the cost
of an unpack in the 8 kHz interrupt. It is not worth it at these sizes; if a
much longer message were needed, that is the first thing to change.

## Validating in Proteus

Put oscilloscope channel C on N5 (before the filter) and channel D on the LM386
output.

| What to check | Expected |
|---|---|
| Idle level at N5 | 2.5 V (code 32) |
| During the ramp part of the test clip | a clean staircase climbing 0 V → 4.9 V in 64 equal 78 mV steps |
| During the 1 kHz tone | a stepped sine, one step every 125 µs |
| After C3 | the same waveform with the step edges rounded off |
| LM386 output | the same shape, larger, centred on about Vcc/2 |

The staircase is the single most useful trace in this project: a missing or
doubled step size means a swapped or stuck bit, and an offset means the PORTD
mask is wrong. Count the steps.
