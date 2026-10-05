#ifndef VAD_H
#define VAD_H

/* Frame-level voice activity detection.
 *
 * The detector works on one number per 8 ms frame - the sum of |sample| over
 * the frame - rather than on individual samples. Keeping the decision out of
 * the ADC interrupt is what lets that interrupt stay at "store one sample and
 * return", which is what makes the 8 ms frame budget hold.
 *
 * The state names below come straight from the assignment: your detector has
 * to be able to tell SILENCE, SPEECH_START, SPEECH_ACTIVE and SPEECH_END
 * apart, and it has to report the two edges the state machine acts on.
 */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/** Where the detector currently thinks it is inside an utterance. */
enum class VadState : uint8_t {
    SILENCE,       /**< below threshold, tracking the noise floor */
    SPEECH_START,  /**< above threshold, not yet confirmed */
    SPEECH_ACTIVE, /**< confirmed speech */
    SPEECH_END,    /**< below threshold, waiting out the hangover */
};

/** Edge reported by vad_process_frame(). */
enum class VadEvent : uint8_t {
    NONE,  /**< nothing changed that the caller needs to act on */
    START, /**< enough consecutive loud frames: an utterance began */
    END,   /**< enough consecutive quiet frames: the utterance finished */
};

/** Clear the state machine and reset the noise floor to its initial estimate. */
void vad_reset(void);

/** Feed one frame's sum-of-|sample| value; returns the edge it produced.
 *
 * Think carefully about when the noise floor may be updated. If you let it
 * track during speech, a long utterance drags the floor up until it mutes
 * itself; if you never update it, the detector cannot adapt to the room.
 */
VadEvent vad_process_frame(uint16_t frame_magnitude);

/** Current detector state, for UART tracing. */
VadState vad_state(void);

/** Current noise-floor estimate, in the same sum-of-|sample| units. */
uint16_t vad_noise_floor(void);

/** Threshold a frame must exceed to count as speech, for UART tracing. */
uint16_t vad_threshold(void);

#endif /* VAD_H */
