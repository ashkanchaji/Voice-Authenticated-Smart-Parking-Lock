#ifndef VAD_H
#define VAD_H

/* Frame-level voice activity detection.
 *
 * The detector works on one number per 8 ms frame - the sum of |sample| over
 * the frame - rather than on individual samples. That keeps the ADC interrupt
 * down to "store one sample and return" and puts every decision in the main
 * loop, which is the whole reason the frame budget holds. The API in the
 * assignment brief was per-sample; this is the same detector moved one level
 * up, and docs/architecture.md explains the trade.
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
    START, /**< VAD_START_FRAMES consecutive loud frames: utterance began */
    END,   /**< VAD_END_FRAMES consecutive quiet frames: utterance finished */
};

/** Clear the state machine and reset the noise floor to VAD_NOISE_INIT. */
void vad_reset(void);

/** Feed one frame's sum-of-|sample| value; returns the edge it produced.
 *
 * The noise floor is only updated while the detector is in SILENCE, so a long
 * utterance cannot drag the floor up and mute itself.
 */
VadEvent vad_process_frame(uint16_t frame_magnitude);

/** Current detector state, for UART tracing. */
VadState vad_state(void);

/** Current noise-floor estimate, in the same sum-of-|sample| units. */
uint16_t vad_noise_floor(void);

/** Threshold a frame must exceed to count as speech, for UART tracing. */
uint16_t vad_threshold(void);

#endif /* VAD_H */
