#include "config.h"
#include "pins.h"
#include "dac_audio.h"
#include "timer2.h"
#include "success_audio.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/* TODO(dac): stream 6-bit PCM from flash to the R-2R ladder on PD2..PD7.
 *
 * success_audio.h is provided: SUCCESS_AUDIO_PCM6[] holds one 0..63 code per
 * byte at SUCCESS_AUDIO_RATE_HZ, and it lives in PROGMEM. Read it with
 * pgm_read_byte(). Do NOT copy it into an SRAM buffer first - the whole point
 * is that a clip costs flash and no RAM.
 *
 * --- Writing a sample ----------------------------------------------------
 * The six ladder bits are PD2..PD7. PD0 and PD1 are the UART, and clobbering
 * them mid-playback corrupts your debug output and changes TXD's idle level.
 * One read-modify-write does it:
 *
 *     PORTD = (PORTD & DAC_KEEP_MASK) | ((code & 0x3F) << DAC_SHIFT);
 *
 * Use the masks from pins.h rather than writing 0x03 and 2 by hand.
 *
 * --- Timing --------------------------------------------------------------
 * Playback is interrupt-driven from Timer2 in PLAYBACK mode, at the same 8 kHz
 * the ADC uses - that is why the two share the timer. Sampling must be stopped
 * before playback starts; they cannot both own Timer2.
 *
 * --- Silence -------------------------------------------------------------
 * The ladder is unipolar: code 0 is 0 V and code 63 is nearly Vcc. Silence is
 * mid-scale, not zero. Park the DAC there when idle and at the end of the clip,
 * or you will hear a click through the amplifier every time playback stops.
 *
 * --- Ending --------------------------------------------------------------
 * The interrupt knows when it has emitted the last sample. Have it retire the
 * timer itself (timer2_stop_isr()) rather than making the main loop poll.
 */

void dac_init(void)
{
    /* TODO: PD2..PD7 as outputs (DAC_PORT_MASK), park the ladder at mid-scale. */
}

void dac_write6(uint8_t code)
{
    (void)code;
    /* TODO: one masked write to PORTD. */
}

void audio_play_success(void)
{
    /* TODO: rewind to sample 0 and put Timer2 into PLAYBACK mode. */
}

void audio_stop(void)
{
    /* TODO: stop the timer if it is in PLAYBACK, park the ladder at mid-scale. */
}

bool audio_is_playing(void)
{
    return false; /* TODO */
}

uint16_t audio_position(void)
{
    return 0; /* TODO */
}

void dac_audio_tick(void)
{
    /* TODO: emit one sample, or finish the clip. Called from the Timer2 ISR,
     * so keep it short. */
}
