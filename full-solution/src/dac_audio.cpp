#include "config.h"
#include "pins.h"
#include "dac_audio.h"
#include "timer2.h"
#include "success_audio.h"
#include <avr/io.h>
#include <avr/interrupt.h>

/** Code 32 of 64 is the middle of the ladder's output range, i.e. silence. */
static const uint8_t DAC_MIDSCALE = 32u;

static volatile uint16_t s_pos;
static volatile bool s_playing;

void dac_init(void)
{
    DAC_DDR |= DAC_PORT_MASK; /* PD2..PD7 out; PD0/PD1 left to the UART */
    dac_write6(DAC_MIDSCALE);
}

void dac_write6(uint8_t code)
{
    /* Single read-modify-write on the port: mask off the six ladder bits,
     * OR in the shifted code, keep PD0/PD1 exactly as they were. */
    DAC_PORT = (uint8_t)((DAC_PORT & DAC_KEEP_MASK) |
                         (uint8_t)((code & DAC_CODE_MASK) << DAC_SHIFT));
}

void audio_play_success(void)
{
    s_pos = 0;
    s_playing = true;
    timer2_set_mode(Timer2Mode::PLAYBACK);
}

void audio_stop(void)
{
    s_playing = false;
    if (timer2_mode() == Timer2Mode::PLAYBACK) {
        timer2_set_mode(Timer2Mode::OFF);
    }
    dac_write6(DAC_MIDSCALE);
}

bool audio_is_playing(void)
{
    return s_playing;
}

uint16_t audio_position(void)
{
    return s_pos;
}

void dac_audio_tick(void)
{
    if (s_pos >= SUCCESS_AUDIO_LEN) {
        /* End of clip: park at mid-scale and stop the timer from inside the
         * interrupt, so the main loop does not have to poll for the end. */
        dac_write6(DAC_MIDSCALE);
        s_playing = false;
        timer2_stop_isr();
        return;
    }
    dac_write6(pgm_read_byte(&SUCCESS_AUDIO_PCM6[s_pos]));
    ++s_pos;
}
