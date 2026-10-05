#ifndef PINS_H
#define PINS_H

/* Pin map for the Arduino Uno R3 / ATmega328P build.
 *
 *   Arduino  Port  Direction  Function
 *   -------  ----  ---------  ------------------------------------------
 *   A0       PC0   analog in  microphone / WAV generator input (ADC0)
 *   A1       PC1   out        green LED, ACCESS GRANTED
 *   A2       PC2   out        red LED, ACCESS DENIED
 *   D0       PD0   in         UART RXD (kept free)
 *   D1       PD1   out        UART TXD (kept free)
 *   D2..D7   PD2..PD7  out    6-bit R-2R ladder DAC, D2 = LSB, D7 = MSB
 *   D8       PB0   in-pullup  training button, active low
 *   D9       PB1   out        servo PWM, OC1A
 *
 * The LEDs deliberately sit on port C rather than on spare port D or B pins:
 * PD2..PD7 are the DAC, PB1 is OC1A, and PB3/PB5 are the SPI/ISP pins that a
 * programmer needs. Port C has room and costs nothing.
 */

#include <avr/io.h>

/* ------------------------------------------------------------- audio in */
#define MIC_ADC_CHANNEL 0u /* ADC0 = PC0 = A0 */

/* ----------------------------------------------------------------- LEDs */
#define LED_PORT PORTC
#define LED_DDR DDRC
#define LED_GREEN_BIT PC1
#define LED_RED_BIT PC2

/* --------------------------------------------------------------- R-2R DAC */
#define DAC_PORT PORTD
#define DAC_DDR DDRD
/** PD2..PD7 carry the ladder; PD0/PD1 must be left alone for the UART. */
#define DAC_PORT_MASK 0xFCu
/** Bits preserved on every DAC write, i.e. the UART pins. */
#define DAC_KEEP_MASK 0x03u
/** Left shift that moves a 6-bit code into PD2..PD7. */
#define DAC_SHIFT 2u
/** The six significant bits of a DAC code, before shifting. */
#define DAC_CODE_MASK 0x3Fu

/* --------------------------------------------------------------- button */
#define BUTTON_PORT PORTB
#define BUTTON_DDR DDRB
#define BUTTON_PIN PINB
#define BUTTON_BIT PB0

/* ---------------------------------------------------------------- servo */
#define SERVO_DDR DDRB
#define SERVO_BIT PB1 /* OC1A */

#endif /* PINS_H */
