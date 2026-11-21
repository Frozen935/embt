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

/** @file @brief Optional shared SBC media-payload playback, outside the A2DP core. */
#ifndef EMBT_AUDIO_A2DP_PLAYER_H_
#define EMBT_AUDIO_A2DP_PLAYER_H_

#include <embt/drivers/audio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_a2dp_player;
/** @brief SBC codec-frame counters and PCM delivery state; read under owner serialization. */
struct bt_a2dp_player_status {
	struct bt_audio_status audio;
	uint64_t decoded_frames;
	uint64_t rejected_packets;
};

/**
 * @brief Create a per-stream SBC decoder and request stereo PCM playback.
 * @param[in] rate Negotiated sample rate in Hz: 16000, 32000, 44100 or 48000.
 * @param[in] backend Registered PCM backend name.
 * @param[in] device Backend device name, or NULL for its default.
 * @param[out] player Owning player pointer, written on acceptance.
 * @return Zero on acceptance or a negative errno.
 * @note Observe READY through get_status before receiving media.
 */
int bt_a2dp_player_open(uint32_t rate, const char *backend, const char *device,
			struct bt_a2dp_player **player);
/**
 * @brief Validate and decode an SBC media payload into PCM.
 * @param[in,out] player Valid owned player; lifecycle and receive calls are serialized by its
 *   owner.
 * @param[in] payload Media header and SBC data; RTP has already been stripped.
 * @param[in] length Payload size in bytes.
 * @param[in] sequence RTP sequence, used for ordered fragment assembly with wrap handling.
 * @param[in] timestamp RTP sample timestamp; fragments of one frame must match.
 * @return Zero on acceptance, -EBADMSG for malformed input, or another negative errno.
 * @note Do not replay a rejected packet: earlier valid frames may already have been submitted.
 * @note Input is borrowed only for this call; incomplete fragments are copied into bounded storage.
 */
int bt_a2dp_player_receive(struct bt_a2dp_player *player, const void *payload,
			   size_t length, uint16_t sequence, uint32_t timestamp);
/**
 * @brief Read decoder counters and synchronized PCM state.
 * @param[in] player Valid owned player; serialize with receive and close.
 * @param[out] status Output counters; must not be NULL.
 * @return Zero on success or a negative errno.
 */
int bt_a2dp_player_get_status(struct bt_a2dp_player *player,
			     struct bt_a2dp_player_status *status);
/**
 * @brief Request playback drain or drop.
 * @param[in,out] player Valid owned player.
 * @param[in] drain True drains queued data; false drops it.
 * @return Zero on acceptance or a negative errno.
 */
int bt_a2dp_player_stop(struct bt_a2dp_player *player, bool drain);
/**
 * @brief Reset fragment and decoder history when restarting stopped playback.
 * @param[in,out] player Valid owned player.
 * @return Zero if restarted or already READY; a negative errno otherwise.
 */
int bt_a2dp_player_start(struct bt_a2dp_player *player);
/**
 * @brief Clear the owning pointer and request asynchronous playback closure.
 * @param[in,out] player Owning pointer; cleared on acceptance and restored on failure.
 * @return Zero on acceptance or a negative errno.
 * @note Decoder storage is released only after PCM CLOSED. Do not use the cleared pointer.
 */
int bt_a2dp_player_close(struct bt_a2dp_player **player);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_AUDIO_A2DP_PLAYER_H_ */
