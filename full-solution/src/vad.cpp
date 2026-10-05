#include "vad.h"

static VadState s_state;
static uint16_t s_noise;
static uint8_t s_run; /* consecutive frames on the current side of the threshold */

void vad_reset(void)
{
    s_state = VadState::SILENCE;
    s_noise = VAD_NOISE_INIT;
    s_run = 0;
}

uint16_t vad_noise_floor(void)
{
    return s_noise;
}

uint16_t vad_threshold(void)
{
    /* 16-bit arithmetic would overflow here for a loud room: a noise floor of
     * 25000 times 3 is 75000. Compute in 32 bits and clamp. */
    uint32_t t = (uint32_t)s_noise * VAD_NOISE_MULT + VAD_ABS_MARGIN;
    return (uint16_t)(t > 0xFFFFul ? 0xFFFFul : t);
}

VadState vad_state(void)
{
    return s_state;
}

VadEvent vad_process_frame(uint16_t frame_magnitude)
{
    const bool loud = frame_magnitude > vad_threshold();

    switch (s_state) {
    case VadState::SILENCE:
        /* Track the noise floor only here, so speech never raises it. */
        if (frame_magnitude > s_noise) {
            s_noise = (uint16_t)(s_noise + ((frame_magnitude - s_noise) >> VAD_NOISE_SHIFT));
        } else {
            s_noise = (uint16_t)(s_noise - ((s_noise - frame_magnitude) >> VAD_NOISE_SHIFT));
        }
        if (loud) {
            s_state = VadState::SPEECH_START;
            s_run = 1;
        }
        break;

    case VadState::SPEECH_START:
        if (!loud) {
            s_state = VadState::SILENCE;
            s_run = 0;
        } else if (++s_run >= VAD_START_FRAMES) {
            s_state = VadState::SPEECH_ACTIVE;
            s_run = 0;
            return VadEvent::START;
        }
        break;

    case VadState::SPEECH_ACTIVE:
        if (!loud) {
            s_state = VadState::SPEECH_END;
            s_run = 1;
        }
        break;

    case VadState::SPEECH_END:
        if (loud) {
            s_state = VadState::SPEECH_ACTIVE;
            s_run = 0;
        } else if (++s_run >= VAD_END_FRAMES) {
            s_state = VadState::SILENCE;
            s_run = 0;
            return VadEvent::END;
        }
        break;
    }

    return VadEvent::NONE;
}
