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

/** @file @brief Validated SBC packets, bounded fragment assembly and stereo PCM submission. */
#include <embt/audio/a2dp_player.h>
#include <embt/bluetooth/sbc.h>
#include <embt/osi/memory.h>
#include <errno.h>

struct bt_a2dp_player {
	struct sbc_decoder decoder;
	struct bt_audio_stream *audio;
	uint32_t rate;
	uint64_t decoded_frames, rejected_packets;
	uint16_t next_sequence;
	uint32_t fragment_timestamp;
	size_t fragment_length;
	bool assembling;
	uint8_t fragment[1024];
	int16_t pcm[256];
};

/** @brief Release decoder storage only after the audio backend relinquishes the stream. */
static void player_event(struct bt_audio_stream *stream, enum bt_audio_state state,
			 int error, void *user)
{
	(void)stream;
	(void)error;
	if (state == BT_AUDIO_CLOSED) {
		osi_free(user);
	}
}

int bt_a2dp_player_open(uint32_t rate, const char *backend, const char *device,
			struct bt_a2dp_player **output)
{
	struct bt_a2dp_player *player;
	struct bt_audio_format format = {.rate = rate, .channels = 2};
	int err;
	if (!output || (rate != 16000 && rate != 32000 && rate != 44100 && rate != 48000)) {
		return -EINVAL;
	}
	player = osi_calloc(1, sizeof(*player));
	if (!player) {
		return -ENOMEM;
	}
	player->rate = rate;
	err = sbc_setup_decoder(&player->decoder);
	if (!err) {
		err = bt_audio_open(backend, device, &format, player_event, player, &player->audio);
	}
	if (err) {
		osi_free(player);
		return err;
	}
	*output = player;
	return 0;
}

/** @brief Compute one validated SBC frame length without allowing decoder resynchronization. */
static int frame_length(const uint8_t *data, size_t length, uint32_t rate)
{
	static const uint32_t rates[] = {16000, 32000, 44100, 48000};
	unsigned int blocks, subbands, mode, channels, bitpool, bits, bytes;
	if (length < 4 || data[0] != 0x9c || rates[data[1] >> 6] != rate) {
		return -EBADMSG;
	}
	blocks = 4U * (((data[1] >> 4) & 3U) + 1U);
	mode = (data[1] >> 2) & 3U;
	channels = mode ? 2U : 1U;
	subbands = (data[1] & 1U) ? 8U : 4U;
	bitpool = data[2];
	if (bitpool < 2 || bitpool > 250 || bitpool > (mode < 2 ? 16U : 32U) * subbands) {
		return -EBADMSG;
	}
	bits = blocks * bitpool * (mode < 2 ? channels : 1U);
	if (mode == 3) {
		bits += subbands;
	}
	bytes = 4U + subbands * channels / 2U + (bits + 7U) / 8U;
	return bytes <= length ? (int)bytes : -EBADMSG;
}

/** @brief Validate every frame before decoding any packet data, then consume all frames. */
static int decode_packet(struct bt_a2dp_player *player, const uint8_t *data,
			 size_t length, unsigned int count)
{
	const uint8_t *cursor = data;
	size_t remaining = length;
	int err;
	for (unsigned int i = 0; i < count; i++) {
		err = frame_length(cursor, remaining, player->rate);
		if (err < 0) {
			return err;
		}
		cursor += err;
		remaining -= err;
	}
	if (remaining) {
		return -EBADMSG;
	}
	for (unsigned int i = 0; i < count; i++) {
		const void *input = data;
		uint32_t input_size = length;
		uint32_t output_size = sizeof(player->pcm);
		int expected = frame_length(data, length, player->rate);
		err = sbc_decode(&player->decoder, &input, &input_size, player->pcm, &output_size);
		if (err || length - input_size != (size_t)expected || !output_size ||
		    output_size % (2U * sizeof(int16_t))) {
			return -EBADMSG;
		}
		player->decoded_frames++;
		err = bt_audio_submit(player->audio, player->pcm, output_size / (2U * sizeof(int16_t)));
		if (err) {
			return err;
		}
		data = input;
		length = input_size;
	}
	return 0;
}

int bt_a2dp_player_receive(struct bt_a2dp_player *player, const void *payload,
			   size_t length, uint16_t sequence, uint32_t timestamp)
{
	const uint8_t *data = payload;
	struct bt_audio_status status;
	uint8_t header;
	int err = -EBADMSG;
	if (!player || !payload) {
		return -EINVAL;
	}
	bt_audio_get_status(player->audio, &status);
	if (status.state != BT_AUDIO_READY) {
		return status.state == BT_AUDIO_OPENING ? -EAGAIN : -ESHUTDOWN;
	}
	if (length < 2) {
		goto reject;
	}
	header = *data++;
	length--;
	if (header & 0x10U) {
		goto reject;
	}
	if (!(header & 0x80U)) {
		player->assembling = false;
		if ((header & 0x60U) || !(header & 0x0fU)) {
			goto reject;
		}
		err = decode_packet(player, data, length, header & 0x0fU);
	} else {
		if ((header & 0x0fU) || (header & 0x60U) == 0x60U) {
			goto reject;
		}
		if (header & 0x40U) {
			player->fragment_length = 0;
			player->fragment_timestamp = timestamp;
			player->assembling = true;
		} else if (!player->assembling || sequence != player->next_sequence ||
			   timestamp != player->fragment_timestamp) {
			goto reject;
		}
		if (length > sizeof(player->fragment) - player->fragment_length) {
			goto reject;
		}
		memcpy(player->fragment + player->fragment_length, data, length);
		player->fragment_length += length;
		player->next_sequence = (uint16_t)(sequence + 1U);
		if (!(header & 0x20U)) {
			return 0;
		}
		player->assembling = false;
		err = decode_packet(player, player->fragment, player->fragment_length, 1);
	}
	if (!err) {
		return 0;
	}
reject:
	player->assembling = false;
	player->rejected_packets++;
	return err;
}

int bt_a2dp_player_get_status(struct bt_a2dp_player *player,
			     struct bt_a2dp_player_status *status)
{
	if (!player || !status) {
		return -EINVAL;
	}
	status->decoded_frames = player->decoded_frames;
	status->rejected_packets = player->rejected_packets;
	return bt_audio_get_status(player->audio, &status->audio);
}

int bt_a2dp_player_stop(struct bt_a2dp_player *player, bool drain)
{
	return player ? bt_audio_stop(player->audio, drain) : -EINVAL;
}

int bt_a2dp_player_start(struct bt_a2dp_player *player)
{
	int err;
	struct bt_audio_status status;
	if (!player) {
		return -EINVAL;
	}
	bt_audio_get_status(player->audio, &status);
	if (status.state != BT_AUDIO_STOPPED) {
		return status.state == BT_AUDIO_READY ? 0 : -EBUSY;
	}
	player->assembling = false;
	err = sbc_setup_decoder(&player->decoder);
	return err ? err : bt_audio_start(player->audio);
}

int bt_a2dp_player_close(struct bt_a2dp_player **player)
{
	struct bt_a2dp_player *current;
	int err;
	if (!player || !*player) {
		return -EINVAL;
	}
	current = *player;
	*player = NULL;
	err = bt_audio_close(current->audio);
	if (err) {
		*player = current;
	}
	return err;
}
