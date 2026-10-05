#ifndef DAC_AUDIO_H
#define DAC_AUDIO_H

/* 6-bit R-2R ladder DAC on PD2..PD7, streamed from flash by Timer2.
 *
 * One PCM byte holds one 0..63 code. Nothing is buffered in SRAM: the Timer2
 * interrupt reads the next byte straight out of PROGMEM and writes the port,
 * so a 0.3 s clip costs 2400 bytes of flash and zero bytes of RAM.
 *
 * PD0 and PD1 are the UART. Every write preserves them (see DAC_KEEP_MASK);
 * clobbering them mid-playback would corrupt the debug output and, worse,
 * change TXD's idle level.
 */

#include <stdint.h>
#include <stdbool.h>

/** Drive PD2..PD7 as outputs and park the ladder at mid-scale. */
void dac_init(void);

/** Write one 6-bit code (0..63) to the ladder, leaving PD0/PD1 untouched. */
void dac_write6(uint8_t code);

/** Start playing the success clip. Requires sampling to be stopped. */
void audio_play_success(void);

/** Stop playback immediately and park the ladder at mid-scale. */
void audio_stop(void);

/** True while a clip is still streaming. */
bool audio_is_playing(void);

/** Samples emitted so far in the current clip, for UART tracing. */
uint16_t audio_position(void);

/** Called from the Timer2 interrupt in PLAYBACK mode. Emits one sample. */
void dac_audio_tick(void);

#endif /* DAC_AUDIO_H */
