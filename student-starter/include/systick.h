#ifndef SYSTICK_H
#define SYSTICK_H

/* Timer0: 1 ms system timebase.
 *
 * CTC mode, prescaler 64, OCR0A = 249:
 *     16 MHz / (64 * (249 + 1)) = 1000 Hz
 *
 * This replaces the Arduino core's millis(), which cannot be used here because
 * the firmware is built without the Arduino framework precisely so that it owns
 * every timer outright.
 */

#include <stdint.h>

/** Configure Timer0 and enable its compare-match interrupt. Call before sei(). */
void systick_init(void);

/** Milliseconds since systick_init(). Reads the 32-bit counter atomically. */
uint32_t systick_ms(void);

/** Elapsed milliseconds since `start`, wrap-safe for the full 32-bit range. */
static inline uint32_t systick_elapsed(uint32_t start)
{
    return systick_ms() - start;
}

#endif /* SYSTICK_H */
