#include "vad.h"

/* TODO(vad): voice activity detection over frame magnitudes.
 *
 * You get one number per 8 ms frame: the sum of |sample| across the frame,
 * which for a +-512 signal is bounded by 64 * 512 = 32768.
 *
 * The detector has to do three things:
 *
 *  1. Estimate the noise floor. A one-pole IIR - move the estimate a fraction
 *     of the way towards each new frame - costs a shift and an add. Decide
 *     when it is allowed to update; see the note in vad.h.
 *
 *  2. Decide "loud" from the noise floor rather than from a fixed number, so
 *     the detector still works when the room changes. A multiplier alone is
 *     not enough: in a silent room the floor tends towards zero and any
 *     multiple of zero is still zero, so you need an absolute margin too.
 *
 *  3. Require several consecutive frames on each side before changing state.
 *     One loud frame is a door slamming, not speech; one quiet frame is the
 *     gap between two words, not the end of the sentence. The hangover on the
 *     way out needs to be long enough to sit through a stop consonant.
 *
 * Report START and END as one-shot events, not levels - the state machine acts
 * on the edges.
 */

void vad_reset(void)
{
    /* TODO: clear the state machine and reset the noise floor. */
}

VadEvent vad_process_frame(uint16_t frame_magnitude)
{
    (void)frame_magnitude;
    return VadEvent::NONE; /* TODO */
}

VadState vad_state(void)
{
    return VadState::SILENCE; /* TODO */
}

uint16_t vad_noise_floor(void)
{
    return 0; /* TODO */
}

uint16_t vad_threshold(void)
{
    return 0; /* TODO */
}
