#include "config.h"
#include "uart.h"
#include <avr/io.h>

void uart_init(void)
{
    /* U2X0 doubles the sampling rate, which halves the rounding error in the
     * divisor. At 16 MHz and 115200 baud:
     *   without U2X0: UBRR = 8  -> 111111 baud, -3.5%
     *   with    U2X0: UBRR = 16 -> 117647 baud, +2.1%
     * 3.5% is close to the ~4% where a UART starts dropping frames, so the
     * doubled-speed mode is not optional here. */
    const uint16_t ubrr = (uint16_t)((F_CPU / (8UL * UART_BAUD)) - 1UL);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0A = _BV(U2X0);
    UCSR0B = _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8 data bits, no parity, 1 stop bit */
}

void uart_putc(char c)
{
    while (!(UCSR0A & _BV(UDRE0))) {
        /* wait for the transmit data register to empty */
    }
    UDR0 = (uint8_t)c;
}

void uart_puts_P(const char *s)
{
    char c;
    while ((c = (char)pgm_read_byte(s++)) != '\0') {
        uart_putc(c);
    }
}

void uart_put_u32(uint32_t value)
{
    char buf[11]; /* 4294967295 is 10 digits, plus the terminator slot */
    uint8_t n = 0;
    do {
        buf[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    while (n != 0u) {
        uart_putc(buf[--n]);
    }
}

void uart_kv(const char *label_P, uint32_t value)
{
    uart_puts_P(label_P);
    uart_puts_P(PSTR(" = "));
    uart_put_u32(value);
    uart_puts_P(PSTR("\r\n"));
}
