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

/** @file @brief Per-channel LC3 decoding, PLC and copied PCM playback. */
#include <embt/audio/bap_player.h>
#include <lc3.h>
#include <embt/base/atomic.h>
#include <embt/osi/memory.h>
#include <errno.h>

struct bt_bap_player {
	struct bt_bap_player_config config;
	struct bt_audio_stream *audio;
	bt_atomic_t refs, closing;
	lc3_decoder_mem_48k_t memory[2];
	lc3_decoder_t decoder[2];
	int16_t pcm[960];
};

struct bt_bap_player *bt_bap_player_ref(struct bt_bap_player *player)
{
	if (player) {
		bt_atomic_inc(&player->refs);
	}
	return player;
}

void bt_bap_player_unref(struct bt_bap_player *player)
{
	if (player && bt_atomic_dec(&player->refs) == 1) {
		osi_free(player);
	}
}

/** @brief Relinquish the backend's reference only after native buffers have been released. */
static void player_event(struct bt_audio_stream *stream, enum bt_audio_state state,
			 int error, void *user)
{
	(void)stream;
	(void)error;
	if (state == BT_AUDIO_CLOSED) {
		bt_bap_player_unref(user);
	}
}

int bt_bap_player_open(const struct bt_bap_player_config *config, const char *backend,
		       const char *device, struct bt_bap_player **output)
{
	struct bt_bap_player *player;
	int err;
	if (!config || !output || !config->blocks || config->blocks > 4 ||
	    !config->octets || config->octets > 400 ||
	    (config->format.channels != 1 && config->format.channels != 2) ||
	    (config->duration_us != 7500 && config->duration_us != 10000) ||
	    (config->format.rate != 8000 && config->format.rate != 16000 &&
	     config->format.rate != 24000 && config->format.rate != 32000 &&
	     config->format.rate != 48000)) {
		return -EINVAL;
	}
	player = osi_calloc(1, sizeof(*player));
	if (!player) {
		return -ENOMEM;
	}
	player->config = *config;
	bt_atomic_set(&player->refs, 2);
	for (size_t channel = 0; channel < config->format.channels; channel++) {
		player->decoder[channel] = lc3_setup_decoder(config->duration_us,
			config->format.rate, 0, &player->memory[channel]);
		if (!player->decoder[channel]) {
			osi_free(player);
			return -EINVAL;
		}
	}
	err = bt_audio_open(backend, device, &config->format, player_event, player, &player->audio);
	if (err) {
		osi_free(player);
		return err;
	}
	*output = player;
	return 0;
}

int bt_bap_player_receive(struct bt_bap_player *player, const void *payload, size_t length, bool plc)
{
	struct bt_audio_status status;
	const uint8_t *data = payload;
	size_t samples;
	int err;
	if (!player) {
		return -EINVAL;
	}
	if (bt_atomic_get(&player->closing)) {
		return -ESHUTDOWN;
	}
	if (!plc && (!payload || length != player->config.octets * player->config.blocks *
					 player->config.format.channels)) {
		return -EBADMSG;
	}
	bt_audio_get_status(player->audio, &status);
	if (status.state != BT_AUDIO_READY) {
		return status.state == BT_AUDIO_OPENING ? -EAGAIN : -ESHUTDOWN;
	}
	samples = (uint64_t)player->config.format.rate * player->config.duration_us / 1000000U;
	for (size_t block = 0; block < player->config.blocks; block++) {
		for (size_t channel = 0; channel < player->config.format.channels; channel++) {
			err = lc3_decode(player->decoder[channel], plc ? NULL : data,
				player->config.octets, LC3_PCM_FORMAT_S16,
				player->pcm + channel, player->config.format.channels);
			if (err < 0) {
				return -EBADMSG;
			}
			if (!plc) {
				data += player->config.octets;
			}
		}
		err = bt_audio_submit(player->audio, player->pcm, samples);
		if (err) {
			return err;
		}
	}
	return 0;
}

int bt_bap_player_get_status(struct bt_bap_player *player, struct bt_audio_status *status)
{
	return player ? bt_audio_get_status(player->audio, status) : -EINVAL;
}

int bt_bap_player_close(struct bt_bap_player **player)
{
	struct bt_bap_player *current;
	int err;
	if (!player || !*player) {
		return -EINVAL;
	}
	current = *player;
	bt_atomic_set(&current->closing, 1);
	*player = NULL;
	err = bt_audio_close(current->audio);
	if (err) {
		*player = current;
		bt_atomic_set(&current->closing, 0);
	} else {
		bt_bap_player_unref(current);
	}
	return err;
}
