#ifndef FEATURE_EXTRACT_H
#define FEATURE_EXTRACT_H

/* Streaming feature extraction.
 *
 * The 1.6 s utterance is 12800 samples, which will never fit in 2 KB of SRAM,
 * so nothing is ever stored at sample rate. Frames arrive one at a time from
 * the ping-pong buffer, each is folded into the accumulators for the segment
 * it belongs to, and the frame is then thrown away. Peak cost is one 64-sample
 * frame plus the 304-byte accumulator block.
 */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "recognizer.h"

/** Sum of |sample| over one frame; the value the VAD runs on.
 *
 * Bounded by 64 * 512 = 32768, so it fits uint16_t with a bit to spare.
 */
uint16_t feature_frame_magnitude(const int16_t *frame);

/** Discard any partial capture and start a new utterance. */
void feature_reset(void);

/** Fold one frame into the accumulators.
 *
 * Returns true once CAPTURE_FRAMES frames have been consumed, i.e. the
 * utterance is complete and feature_finalize() can be called. Extra calls
 * after that are ignored.
 */
bool feature_process_frame(const int16_t *frame);

/** Frames consumed so far in the current capture, 0..CAPTURE_FRAMES. */
uint16_t feature_frames_done(void);

/** Normalise the accumulators into FEATURE_COUNT 0..255 codes. */
void feature_finalize(uint8_t *out);

/** Read-only view of the raw accumulators, for UART diagnostics. */
const RawFeatures *feature_raw(void);

/** First FFT bin of band `b` (inclusive). Exposed for documentation checks. */
uint8_t feature_band_start(uint8_t b);

/** Last FFT bin of band `b` (inclusive). */
uint8_t feature_band_end(uint8_t b);

#endif /* FEATURE_EXTRACT_H */
