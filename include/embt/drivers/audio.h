/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/** @file @brief Portable asynchronous S16 PCM playback. */
#ifndef EMBT_DRIVERS_AUDIO_H_
#define EMBT_DRIVERS_AUDIO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_audio_stream;

/** @brief Asynchronous PCM lifecycle; ERROR still requires an explicit close. */
enum bt_audio_state {
	BT_AUDIO_CLOSED, BT_AUDIO_OPENING, BT_AUDIO_READY,
	BT_AUDIO_STOPPING, BT_AUDIO_STOPPED, BT_AUDIO_CLOSING, BT_AUDIO_ERROR,
};

/** @brief One PCM frame contains one native-endian S16 sample per channel. */
struct bt_audio_format {
	uint32_t rate;
	uint8_t channels;
};

/** @brief Synchronized stream state and counters measured in PCM frames. */
struct bt_audio_status {
	enum bt_audio_state state;
	int error;
	uint64_t submitted_frames;
	uint64_t consumed_frames; /**< Copied by the backend, not necessarily audible. */
	size_t queued_frames;
};

/**
 * @brief Observe an asynchronous native completion without waiting on audio work.
 * @param[in] stream Completing PCM stream; valid throughout the callback.
 * @param[in] state Completed lifecycle state.
 * @param[in] error Zero on success or a negative native error.
 * @param[in,out] user Borrowed context supplied to open.
 * @note Called outside the stream lock; do not synchronously wait for the backend.
 */
typedef void (*bt_audio_notify_t)(struct bt_audio_stream *stream,
				enum bt_audio_state state, int error, void *user);

/**
 * @brief Initialize audio slots before registration or concurrent use.
 * @return Zero on success, -EALREADY if initialized, or a negative OSI error.
 */
int bt_audio_driver_init(void);
/**
 * @brief Close all streams and join their backend execution.
 * @note Call from the application control thread, after stopping producers.
 * @note Serialize with initialization, registration and all stream calls.
 */
void bt_audio_driver_shutdown(void);
/**
 * @brief Register PulseAudio without connecting to an audio server.
 * @return Zero on success, or a negative error for invalid state, duplicates or resource failure.
 */
int bt_audio_pulse_register(void);
/**
 * @brief Request a playback stream on a named backend.
 * @param[in] backend Registered backend name; must not be NULL.
 * @param[in] device Backend device name, or NULL for its default.
 * @param[in] format Native S16 rate/channel configuration; must not be NULL.
 * @param[in] notify Optional asynchronous state notification.
 * @param[in] user Borrowed callback context, retained until CLOSED notification returns.
 * @param[out] stream Handle published before notifications; output changes only on success.
 * @return Zero if accepted, -ENODEV for an unknown backend, or another negative errno.
 * @note Observe READY before PCM submission. Acceptance does not mean the device is ready.
 * @note Serialize initialization, registration and open; stream operations use per-stream locking.
 * @note Do not reuse a closed handle after its slot has been made available to another open.
 */
int bt_audio_open(const char *backend, const char *device,
		  const struct bt_audio_format *format, bt_audio_notify_t notify,
		  void *user, struct bt_audio_stream **stream);
/**
 * @brief Copy one complete PCM block without waiting for device capacity.
 * @param[in] stream Valid owned PCM stream.
 * @param[in] pcm Interleaved native S16 samples; reusable after return.
 * @param[in] frames PCM frames, each containing one sample per channel; must be positive.
 * @return Zero on complete enqueue, -EAGAIN for pressure/opening, -EMSGSIZE for a large block, or
 *   another negative errno.
 */
int bt_audio_submit(struct bt_audio_stream *stream, const int16_t *pcm, size_t frames);
/**
 * @brief Request asynchronous drain or drop.
 * @param[in,out] stream Valid owned READY stream.
 * @param[in] drain True preserves queued PCM; false discards queued and device-buffered PCM.
 * @return Zero on acceptance, -EBUSY for an incompatible state, or another negative errno.
 * @note STOPPED notification confirms completion. Further submissions are rejected after
 *   acceptance.
 */
int bt_audio_stop(struct bt_audio_stream *stream, bool drain);
/**
 * @brief Resume a fully stopped stream on its existing backend.
 * @param[in,out] stream Valid owned STOPPED stream.
 * @return Zero on acceptance, -EBUSY for an incompatible state, or another negative errno.
 */
int bt_audio_start(struct bt_audio_stream *stream);
/**
 * @brief Asynchronously drop PCM and release a playback stream.
 * @param[in,out] stream Valid owned stream, including an ERROR stream.
 * @return Zero on acceptance or a negative errno.
 * @note Release callback context only after CLOSED notification returns.
 */
int bt_audio_close(struct bt_audio_stream *stream);
/**
 * @brief Read synchronized PCM state and delivery counters.
 * @param[in] stream Valid owned stream; may be observed until its closure is acknowledged.
 * @param[out] status State and PCM frame counts; must not be NULL.
 * @return Zero on success or -EINVAL for an invalid request.
 */
int bt_audio_get_status(struct bt_audio_stream *stream, struct bt_audio_status *status);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_DRIVERS_AUDIO_H_ */
