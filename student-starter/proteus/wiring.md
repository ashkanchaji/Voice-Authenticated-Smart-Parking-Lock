# Proteus wiring guide

Every connection in the circuit, net by net. Draw them in this order and the
schematic will come out readable.

Place the parts from [BOM.csv](BOM.csv) first. Wherever a net appears in more
than one block below, use a **named net label** rather than a long wire — the
ladder in particular becomes unreadable if you route it by hand.

---

## 1. Power

| From | To |
|---|---|
| `U1` 5V pin | `+5V` power rail |
| `U1` GND pin | `GND` power rail |
| `U2` (LM386) pin 6 | `+5V` |
| `U2` (LM386) pin 4 | `GND` |
| `U2` (LM386) pin 2 | `GND` |
| `C6` (100 nF) | between `+5V` and `GND`, next to `U2` |
| `M1` (servo) `+V` | `+5V` |
| `M1` (servo) `GND` | `GND` |

In Proteus you can also place a `POWER` terminal labelled `+5V` and a `GROUND`
terminal rather than running rails; either is fine as long as every block above
is connected.

---

## 2. Audio input into A0

```
   G1 (AUDIO generator)
        |
        |          C1 = 1 uF
        +----------||----------+------------------+
                               |                  |
                              R1 100k            R2 100k
                               |                  |
                             +5V                 GND
                               |
                          (node BIAS)
                               |
                              R3 4.7k
                               |
                         +-----+-----> U1 pin A0     <- net MIC_IN
                         |
                        C2 10nF
                         |
                        GND
```

Step by step:

1. Right-click an empty wire end → **Place → Generator → AUDIO**, name it
   `AUDIO_IN`, and attach it to one end of `C1`.
2. In the generator's properties, set **WAV Audio File** to
   `assets/scenario.wav` (produced by `tools/prepare_voice_dataset.py`).
   Set the amplitude so the trace at A0 never reaches 0 V or 5 V — start at
   **2 V peak-to-peak** and check on the scope.
3. `C1` other end → node `BIAS`.
4. `R1` (100 k) from `BIAS` to `+5V`.
5. `R2` (100 k) from `BIAS` to `GND`.
6. `R3` (4.7 k) from `BIAS` to node `MIC_IN`.
7. `C2` (10 nF) from `MIC_IN` to `GND`.
8. `MIC_IN` → `U1` pin `A0`.

What each part does:

| Part | Purpose | Number |
|---|---|---|
| `C1` | blocks the source's DC so the bias sets the operating point | corner with R1‖R2 = 3.2 Hz |
| `R1`/`R2` | 2.5 V bias, so silence reads ADC ≈ 512 | 5 V × 100k/(100k+100k) |
| `R3`/`C2` | anti-alias low-pass | 1/(2π·4.7k·10n) = **3.39 kHz** |

**The bias is not optional.** The ADC cannot read a negative voltage; without
the offset the whole negative half of the waveform is lost.

---

## 3. Servo on D9

| From | To |
|---|---|
| `U1` pin `D9` | `M1` (MOTOR-PWMSERVO) PWM input — net `SERVO_PWM` |
| `M1` `+V` | `+5V` |
| `M1` `GND` | `GND` |

In `M1`'s properties, leave the default minimum/maximum pulse widths (1 ms /
2 ms) so the model matches the firmware's `SERVO_PULSE_LOCKED` = 2000 ticks and
`SERVO_PULSE_UNLOCKED` = 4000 ticks (0.5 µs per tick).

---

## 4. The 6-bit R-2R ladder on D2..D7

```
 GND
  |
 R16
 20k      D2        D3        D4        D5        D6        D7
  |      (LSB)                                             (MSB)
  |       |         |         |         |         |         |
  |      R10       R11       R12       R13       R14       R15
  |      20k       20k       20k       20k       20k       20k
  |       |         |         |         |         |         |
  +--N0---+--R17----N1--R18---N2--R19---N3--R20---N4--R21---N5 ---> DAC_OUT
             10k        10k       10k       10k       10k
```

Net list, exactly:

| Component | From | To |
|---|---|---|
| `R16` 20k | `GND` | `N0` |
| `R10` 20k | `U1` `D2` | `N0` |
| `R17` 10k | `N0` | `N1` |
| `R11` 20k | `U1` `D3` | `N1` |
| `R18` 10k | `N1` | `N2` |
| `R12` 20k | `U1` `D4` | `N2` |
| `R19` 10k | `N2` | `N3` |
| `R13` 20k | `U1` `D5` | `N3` |
| `R20` 10k | `N3` | `N4` |
| `R14` 20k | `U1` `D6` | `N4` |
| `R21` 10k | `N4` | `N5` |
| `R15` 20k | `U1` `D7` | `N5` |
| — | `N5` | net `DAC_OUT` |

**Check twice:** D2 is the LSB and D7 is the MSB. Swapping the ends produces a
DAC that still "works" — it just outputs a mirrored, scrambled waveform, which
is much harder to diagnose than a dead one. The staircase test in
[validation-checklist.md](validation-checklist.md) catches it immediately.

Looking into `N5`, the ladder presents a constant 10 kΩ regardless of the code.
Output is `Vout = 5 V × code / 64`, one LSB = 78.1 mV.

---

## 5. Output stage: filter, volume, amplifier, speaker

| Component | From | To | Purpose |
|---|---|---|---|
| `C3` 4.7 nF | `DAC_OUT` | `GND` | reconstruction filter; with the ladder's 10 kΩ this corners at **3.39 kHz** |
| `C4` 10 µF | `DAC_OUT` | `RV1` top | AC coupling, removes the 2.5 V idle offset |
| `RV1` 10k | top / wiper / bottom | wiper → `U2` pin 3, bottom → `GND` | volume |
| `U2` pin 3 | from `RV1` wiper | — | LM386 non-inverting input |
| `U2` pin 2 | `GND` | — | inverting input |
| `U2` pin 5 | `C5` + terminal | — | output |
| `C5` 220 µF | `U2` pin 5 | `LS1` + | output coupling |
| `LS1` − | `GND` | — | speaker return |

Leave LM386 pins 1 and 8 unconnected: that is its default gain of 20, which is
plenty here.

The 10 kΩ pot loads the 10 kΩ ladder and halves the signal at full volume. That
is expected. Use a 100 kΩ pot if you want the full swing.

---

## 6. Training button on D8

| From | To |
|---|---|
| `U1` pin `D8` | `SW1` terminal 1 — net `BTN_TRAIN` |
| `SW1` terminal 2 | `GND` |

No external pull-up: the firmware enables the ATmega328P's internal one
(`PORTB |= _BV(PB0)`), so the pin idles high and reads low when pressed.

---

## 7. LEDs on A1 and A2

| From | To |
|---|---|
| `U1` pin `A1` | `R4` 330 Ω → `D1` (green) anode |
| `D1` cathode | `GND` |
| `U1` pin `A2` | `R5` 330 Ω → `D2` (red) anode |
| `D2` cathode | `GND` |

---

## 8. Instruments

**Virtual terminal** (`X2`):

| From | To |
|---|---|
| `U1` pin `D1` (TXD) | `X2` `RXD` |

Properties: **115200** baud, **8** data bits, **none** parity, **1** stop bit.
Leave `X2` `TXD` unconnected — the firmware never receives.

**Oscilloscope** (`X1`), four channels:

| Channel | Net | What you will see |
|---|---|---|
| A | `MIC_IN` (A0) | audio riding on 2.5 V |
| B | `SERVO_PWM` (D9) | 50 Hz PWM, 1 ms → 2 ms |
| C | `DAC_OUT` (N5) | 64-step staircase |
| D | `U2` pin 5 | the amplified, filtered waveform |

Suggested initial settings: 5 ms/div for the servo, 100 µs/div for the DAC
staircase, 1 V/div on all channels.

---

## 9. Loading the firmware

Double-click `U1`, then set:

| Property | Value |
|---|---|
| Program File | `.pio/build/uno/firmware.hex` |
| Clock Frequency | 16 MHz |

Use `firmware.elf` instead if you want source-level debugging inside Proteus;
both are produced by `pio run`.

---

## Common wiring mistakes

| Symptom | Likely cause |
|---|---|
| A0 trace centred on 0 V | `C1` or the `R1`/`R2` divider missing |
| A0 trace clipped flat top and bottom | generator amplitude too high |
| No UART output | `X2` baud rate wrong, or wired to D0 instead of D1 |
| UART garbage during audio playback only | DAC write not masking PD0/PD1 |
| DAC staircase mirrored or scrambled | D2/D7 swapped — LSB and MSB reversed |
| DAC steps of unequal height | a 10 k and a 20 k swapped somewhere in the ladder |
| Servo never moves but D9 shows PWM | `M1` pulse-width limits changed from the defaults |
| Everything works, then `ADC OVERRUN = n` appears | not a wiring fault — see [../docs/timers.md](../docs/timers.md) |
