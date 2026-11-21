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

/** @file @brief Real SBC encode/decode packets, fragmentation and malformed media tests. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "fake.h"
#include <embt/audio/a2dp_player.h>
#include <embt/bluetooth/sbc.h>
#include <errno.h>

/** @brief Generate one real codec frame with deterministic mono or stereo PCM. */
static size_t encode(uint8_t *frame, bool mono)
{
	struct sbc_encoder encoder;
	struct sbc_encoder_init_param config = {
		.bit_rate = mono ? 128 : 328, .samp_freq = 48000,
		.blk_len = 16, .subband = 8, .alloc_mthd = SBC_ALLOC_MTHD_LOUDNESS,
		.ch_mode = mono ? SBC_CH_MODE_MONO : SBC_CH_MODE_JOINT_STEREO,
		.ch_num = mono ? 1 : 2, .min_bitpool = 2, .max_bitpool = 53,
	};
	int16_t pcm[256];
	assert_int_equal(sbc_setup_encoder(&encoder, &config), 0);
	for (size_t i = 0; i < 256; i++) {
		pcm[i] = ((int)(i % 32) - 16) * 700;
	}
	return sbc_encode(&encoder, pcm, frame);
}

/** @brief Decode all three packet frames and verify actual PCM delivery. */
static void multi_frame(void **state)
{
	struct bt_a2dp_player *player;
	struct bt_a2dp_player_status status;
	uint8_t packet[1024] = {3};
	size_t size = encode(packet + 1, false);
	(void)state;
	memcpy(packet + 1 + size, packet + 1, size);
	memcpy(packet + 1 + 2 * size, packet + 1, size);
	assert_int_equal(bt_a2dp_player_open(48000, "memory", NULL, &player), 0);
	assert_int_equal(bt_a2dp_player_receive(player, packet, 1 + 3 * size, 0, 0), 0);
	bt_a2dp_player_get_status(player, &status);
	assert_int_equal(status.decoded_frames, 3);
	assert_int_equal(status.audio.consumed_frames, 384);
	assert_int_equal(fake_length, 384 * 4);
	assert_int_equal(bt_a2dp_player_close(&player), 0);
	assert_null(player);
}

/** @brief Mono decoding retains the existing decoder's stereo expansion contract. */
static void mono(void **state)
{
	struct bt_a2dp_player *player;
	uint8_t packet[1024] = {1};
	size_t size = encode(packet + 1, true);
	(void)state;
	assert_int_equal(bt_a2dp_player_open(48000, "memory", NULL, &player), 0);
	assert_int_equal(bt_a2dp_player_receive(player, packet, size + 1, 0, 0), 0);
	assert_int_equal(fake_length, 128 * 4);
	for (size_t i = 0; i < fake_length; i += 4) {
		assert_memory_equal(fake_pcm + i, fake_pcm + i + 2, 2);
	}
	assert_int_equal(bt_a2dp_player_close(&player), 0);
}

/** @brief Reject malformed frame counts, truncation, reserved bits and wrong sample rate. */
static void malformed(void **state)
{
	struct bt_a2dp_player *player;
	struct bt_a2dp_player_status status;
	uint8_t packet[1024] = {1};
	size_t size = encode(packet + 1, false);
	(void)state;
	assert_int_equal(bt_a2dp_player_open(48000, "memory", NULL, &player), 0);
	assert_int_equal(bt_a2dp_player_receive(player, packet, 1, 0, 0), -EBADMSG);
	assert_int_equal(bt_a2dp_player_receive(player, packet, size, 0, 0), -EBADMSG);
	packet[0] = 2;
	assert_int_equal(bt_a2dp_player_receive(player, packet, size + 1, 0, 0), -EBADMSG);
	packet[0] = 0x11;
	assert_int_equal(bt_a2dp_player_receive(player, packet, size + 1, 0, 0), -EBADMSG);
	bt_a2dp_player_get_status(player, &status);
	assert_int_equal(status.audio.submitted_frames, 0);
	assert_int_equal(status.rejected_packets, 4);
	assert_int_equal(bt_a2dp_player_close(&player), 0);
}

/** @brief Reassemble one frame across sequence wrap and reject a lost middle fragment. */
static void fragments(void **state)
{
	struct bt_a2dp_player *player;
	uint8_t frame[1024], first[1024], last[1024];
	size_t size = encode(frame, false), half = size / 2;
	(void)state;
	first[0] = 0xc0;
	last[0] = 0xa0;
	memcpy(first + 1, frame, half);
	memcpy(last + 1, frame + half, size - half);
	assert_int_equal(bt_a2dp_player_open(48000, "memory", NULL, &player), 0);
	assert_int_equal(bt_a2dp_player_receive(player, first, half + 1, 65535, 123), 0);
	assert_int_equal(fake_length, 0);
	assert_int_equal(bt_a2dp_player_receive(player, last, size - half + 1, 0, 123), 0);
	assert_int_equal(fake_length, 512);
	assert_int_equal(bt_a2dp_player_receive(player, first, half + 1, 1, 124), 0);
	assert_int_equal(bt_a2dp_player_receive(player, last, size - half + 1, 3, 124), -EBADMSG);
	assert_int_equal(fake_length, 512);
	assert_int_equal(bt_a2dp_player_close(&player), 0);
}

/** @brief Preserve decoder CRC rejection and reset state when playback resumes. */
static void crc_and_restart(void **state)
{
	struct bt_a2dp_player *player;
	uint8_t packet[1024] = {1};
	size_t size = encode(packet + 1, false);
	(void)state;
	assert_int_equal(bt_a2dp_player_open(48000, "memory", NULL, &player), 0);
	packet[4] ^= 0xff;
	assert_int_equal(bt_a2dp_player_receive(player, packet, size + 1, 0, 0), -EBADMSG);
	assert_int_equal(fake_length, 0);
	packet[4] ^= 0xff;
	assert_int_equal(bt_a2dp_player_stop(player, false), 0);
	assert_int_equal(bt_a2dp_player_receive(player, packet, size + 1, 1, 128), -ESHUTDOWN);
	assert_int_equal(bt_a2dp_player_start(player), 0);
	assert_int_equal(bt_a2dp_player_receive(player, packet, size + 1, 2, 256), 0);
	assert_int_equal(fake_length, 512);
	assert_int_equal(bt_a2dp_player_close(&player), 0);
}

/** @brief Run the shared playback adapter against the real installed codec provider. */
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(multi_frame, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(mono, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(malformed, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(fragments, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(crc_and_restart, audio_setup, audio_teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
