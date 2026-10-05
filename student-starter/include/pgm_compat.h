#ifndef PGM_COMPAT_H
#define PGM_COMPAT_H

/* Flash-string / flash-table access.
 *
 * On the AVR the constant tables (window, twiddles, audio, weights) live in
 * flash and are read with the LPM instruction, which is what avr/pgmspace.h
 * wraps. The host test build has a flat address space, so the same names
 * become plain dereferences and the DSP code compiles unchanged for both.
 */

#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#include <stdint.h>
#define PROGMEM
#define pgm_read_byte(a) (*(const uint8_t *)(a))
#define pgm_read_word(a) (*(const uint16_t *)(a))
#define PSTR(s) (s)
#endif

#endif /* PGM_COMPAT_H */
