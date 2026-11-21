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

/** @file @brief Optional LC3 SDU playback outside the Bluetooth BAP core. */
#ifndef EMBT_AUDIO_BAP_PLAYER_H_
#define EMBT_AUDIO_BAP_PLAYER_H_
#include <embt/drivers/audio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_bap_player;
/** @brief LC3 configuration for mono or interleaved stereo SDUs. */
struct bt_bap_player_config {
	struct bt_audio_format format;
	uint32_t duration_us; /**< Codec duration: 7500 or 10000 microseconds. */
	uint16_t octets; /**< Encoded bytes per channel per temporal block. */
	uint8_t blocks; /**< Temporal blocks per SDU, from one to four. */
};

/**
 * @brief Create independent per-channel LC3 decoders and request PCM playback.
 * @param[in] config Mono/stereo LC3 rate, duration, frame octets and temporal block count.
 * @param[in] backend Registered PCM backend name.
 * @param[in] device Backend device name, or NULL for its default.
 * @param[out] player Owning player pointer, written on acceptance.
 * @return Zero on acceptance or a negative errno.
 * @note Observe READY before receiving SDUs.
 */
int bt_bap_player_open(const struct bt_bap_player_config *config, const char *backend,
		       const char *device, struct bt_bap_player **player);
/**
 * @brief Decode one LC3 SDU or conceal its missing temporal blocks.
 * @param[in,out] player Retained player; serialize decoder use with other receive calls.
 * @param[in] payload Encoded LC3 frames, ordered by block then channel; ignored for PLC.
 * @param[in] length SDU size in bytes; must exactly match configuration when plc is false.
 * @param[in] plc True conceals missing frames without reading payload.
 * @return Zero on success, -EBADMSG for invalid data, -ESHUTDOWN for a closing player, or another
 *   negative errno.
 * @note PCM is copied before return. Do not replay an SDU after a partial enqueue failure.
 */
int bt_bap_player_receive(struct bt_bap_player *player, const void *payload, size_t length, bool plc);
/**
 * @brief Read synchronized PCM state and delivery counters.
 * @param[in] player Valid retained player.
 * @param[out] status State and PCM frame counts; must not be NULL.
 * @return Zero on success or a negative errno.
 */
int bt_bap_player_get_status(struct bt_bap_player *player, struct bt_audio_status *status);
/**
 * @brief Retain a player while an encoded SDU is queued.
 * @param[in,out] player Valid retained player, or NULL.
 * @return The retained pointer, or NULL for NULL input.
 * @note Serialize acquisition of the owning pointer with owner close.
 */
struct bt_bap_player *bt_bap_player_ref(struct bt_bap_player *player);
/**
 * @brief Release one owning or queued-job reference.
 * @param[in,out] player Previously retained player, or NULL.
 * @note Final release frees decoder storage; do not use the pointer afterwards.
 */
void bt_bap_player_unref(struct bt_bap_player *player);
/**
 * @brief Clear ownership and defer release until PCM and queued jobs complete.
 * @param[in,out] player Owning pointer; cleared on acceptance and restored on failure.
 * @return Zero on acceptance or a negative errno.
 * @note Serialize owner close with new reference acquisition; already retained jobs remain valid.
 */
int bt_bap_player_close(struct bt_bap_player **player);
#ifdef __cplusplus
}
#endif

#endif /* EMBT_AUDIO_BAP_PLAYER_H_ */
