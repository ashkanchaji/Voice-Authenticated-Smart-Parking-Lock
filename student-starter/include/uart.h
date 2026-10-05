#ifndef UART_H
#define UART_H

/* Minimal blocking debug UART.
 *
 * Transmit only, no ring buffer, no stdio. That costs about 1.5 KB of flash
 * and 150 bytes of SRAM less than pulling in avr-libc's printf, and this
 * firmware never needs formatted output - it prints fixed labels and unsigned
 * integers.
 *
 * Blocking matters: at 115200 baud one byte takes 87 us and a 40-character
 * line takes 3.5 ms, which is nearly half of the 8 ms frame budget. The state
 * machine therefore prints only at state transitions, never inside the capture
 * loop. See docs/timers.md.
 */

#include <stdint.h>
#include "pgm_compat.h"

/** Configure USART0 for UART_BAUD, 8N1, transmitter enabled. */
void uart_init(void);

/** Send one byte, blocking until the transmit buffer is free. */
void uart_putc(char c);

/** Send a NUL-terminated string held in flash. Use with PSTR("..."). */
void uart_puts_P(const char *s);

/** Send an unsigned decimal number. */
void uart_put_u32(uint32_t value);

/** Send a flash label, then ": ", then a number, then CRLF. */
void uart_kv(const char *label_P, uint32_t value);

/** Shorthand for uart_puts_P(PSTR(s)) followed by CRLF. */
#define UART_LINE(s)          \
    do {                      \
        uart_puts_P(PSTR(s)); \
        uart_puts_P(PSTR("\r\n")); \
    } while (0)

#endif /* UART_H */
