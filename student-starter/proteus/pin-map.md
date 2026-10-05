# Pin map — Arduino Uno R3 / ATmega328P

| Arduino | AVR port | Direction | Net name | Function |
|---|---|---|---|---|
| A0 | PC0 / ADC0 | analogue in | `MIC_IN` | voice input from the WAV generator |
| A1 | PC1 | digital out | `LED_GRN` | green LED — ACCESS GRANTED |
| A2 | PC2 | digital out | `LED_RED` | red LED — ACCESS DENIED |
| A3 | PC3 | — | — | free |
| A4 | PC4 / SDA | — | — | free |
| A5 | PC5 / SCL | — | — | free |
| D0 | PD0 / RXD | in | `UART_RX` | UART receive — **reserved, do not use** |
| D1 | PD1 / TXD | out | `UART_TX` | UART transmit to the virtual terminal |
| D2 | PD2 | digital out | `DAC0` | R-2R ladder, **LSB** (weight 1) |
| D3 | PD3 | digital out | `DAC1` | R-2R ladder (weight 2) |
| D4 | PD4 | digital out | `DAC2` | R-2R ladder (weight 4) |
| D5 | PD5 | digital out | `DAC3` | R-2R ladder (weight 8) |
| D6 | PD6 | digital out | `DAC4` | R-2R ladder (weight 16) |
| D7 | PD7 | digital out | `DAC5` | R-2R ladder, **MSB** (weight 32) |
| D8 | PB0 | in, pull-up | `BTN_TRAIN` | training button, active low |
| D9 | PB1 / OC1A | digital out | `SERVO_PWM` | 50 Hz servo PWM |
| D10 | PB2 / SS | — | — | free |
| D11 | PB3 / MOSI | — | — | free (ISP) |
| D12 | PB4 / MISO | — | — | free (ISP) |
| D13 | PB5 / SCK | — | — | free (ISP, on-board LED) |

## Why these pins

* **A0 for audio.** ADC0 is the lowest channel and the one the firmware selects
  with `ADMUX = _BV(REFS0) | 0`. Its digital input buffer is disabled in
  `DIDR0` so the switching CMOS stage cannot inject noise into the sample.
* **D2..D7 for the ladder.** They are the top six bits of one port, so a whole
  sample is written with a single `PORTD` read-modify-write. Spreading the
  ladder across two ports would need two writes and would introduce a skew
  between the bits — a visible glitch on every code transition.
* **D0/D1 stay clear.** They are the UART. The DAC write masks them explicitly
  (`PORTD & 0x03`) so playback cannot corrupt the debug trace or change TXD's
  idle level.
* **D9 for the servo.** It is OC1A, the compare output of Timer1, which is the
  only 16-bit timer on this chip and therefore the only one that can produce a
  20 ms period with sub-microsecond pulse resolution.
* **A1/A2 for the LEDs.** Port C has room; every spare port D pin is in the
  ladder and every spare port B pin is either OC1A or an ISP signal.
* **D8 for the button.** PB0 has no alternate function this project needs, and
  it has an internal pull-up, so the switch needs no external resistor.

## Timer ownership

| Timer | Owner | Job |
|---|---|---|
| Timer0 | `systick.cpp` | 1 ms system tick |
| Timer1 | `servo.cpp` | 50 Hz PWM on OC1A (D9) |
| Timer2 | `timer2.cpp` | 8 kHz clock, shared between ADC sampling and DAC playback |

The firmware is built without the Arduino framework, so `millis()` and
`delay()` do not exist and Timer0 belongs entirely to this project.
