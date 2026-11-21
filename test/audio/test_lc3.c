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

/** @file @brief Real LC3 mono/stereo SDUs, PLC and queued decoder lifetime. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "fake.h"
#include <embt/audio/bap_player.h>
#include <lc3.h>
#include <errno.h>
#include <string.h>

/** @brief Encode two channels and two temporal blocks, preserving independent codec history. */
static void stereo_blocks(void **state)
{
	struct bt_bap_player *player;
	struct bt_bap_player_config config = {{48000, 2}, 10000, 100, 2};
	lc3_encoder_mem_48k_t memory[2];
	lc3_encoder_t encoder[2];
	int16_t pcm[480];
	uint8_t sdu[400];
	struct bt_audio_status status;
	(void)state;
	for (size_t channel = 0; channel < 2; channel++) {
		encoder[channel] = lc3_setup_encoder(10000, 48000, 0, &memory[channel]);
		assert_non_null(encoder[channel]);
	}
	for (size_t block = 0; block < 2; block++) {
		for (size_t channel = 0; channel < 2; channel++) {
			for (size_t i = 0; i < 480; i++) {
				pcm[i] = ((int)(i % (channel ? 20 : 32)) - 10) * 600;
			}
			assert_int_equal(lc3_encode(encoder[channel], LC3_PCM_FORMAT_S16,
				pcm, 1, 100, sdu + (block * 2 + channel) * 100), 0);
		}
	}
	assert_int_equal(bt_bap_player_open(&config, "memory", NULL, &player), 0);
	assert_int_equal(bt_bap_player_receive(player, sdu, sizeof(sdu) - 1, false), -EBADMSG);
	assert_int_equal(fake_length, 0);
	assert_int_equal(bt_bap_player_receive(player, sdu, sizeof(sdu), false), 0);
	bt_bap_player_get_status(player, &status);
	assert_int_equal(status.consumed_frames, 960);
	assert_int_equal(fake_length, 960 * 4);
	{
		size_t differing = 0;
		for (size_t i = 0; i < fake_length; i += 4) {
			differing += memcmp(fake_pcm + i, fake_pcm + i + 2, 2) != 0;
		}
		assert_true(differing > 100);
	}
	assert_int_equal(bt_bap_player_close(&player), 0);
}

/** @brief Conceal an absent mono SDU with exactly its negotiated sample count. */
static void mono_plc(void **state)
{
	struct bt_bap_player *player;
	struct bt_bap_player_config config = {{16000, 1}, 7500, 40, 1};
	(void)state;
	assert_int_equal(bt_bap_player_open(&config, "memory", NULL, &player), 0);
	assert_int_equal(bt_bap_player_receive(player, NULL, 0, true), 0);
	assert_int_equal(fake_length, 120 * 2);
	assert_int_equal(bt_bap_player_close(&player), 0);
}

/** @brief Keep queued contexts alive through close and reject stale PCM after slot reuse. */
static void job_lifetime(void **state)
{
	struct bt_bap_player *player, *job, *replacement;
	struct bt_bap_player_config config = {{16000, 1}, 10000, 40, 1};
	(void)state;
	assert_int_equal(bt_bap_player_open(&config, "memory", NULL, &player), 0);
	job = bt_bap_player_ref(player);
	assert_int_equal(bt_bap_player_close(&player), 0);
	assert_null(player);
	assert_int_equal(bt_bap_player_open(&config, "memory", NULL, &replacement), 0);
	assert_int_equal(bt_bap_player_receive(job, NULL, 0, true), -ESHUTDOWN);
	assert_int_equal(fake_length, 0);
	bt_bap_player_unref(job);
	assert_int_equal(bt_bap_player_close(&replacement), 0);
}

/** @brief Reject unsupported multichannel and frame duration without consuming PCM slots. */
static void invalid_config(void **state)
{
	struct bt_bap_player *player;
	struct bt_bap_player_config config = {{48000, 3}, 10000, 100, 1};
	(void)state;
	assert_int_equal(bt_bap_player_open(&config, "memory", NULL, &player), -EINVAL);
	config.format.channels = 1;
	config.duration_us = 15000;
	assert_int_equal(bt_bap_player_open(&config, "memory", NULL, &player), -EINVAL);
}

/** @brief Run the real LC3 codec through the portable playback interface. */
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(stereo_blocks, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(mono_plc, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(job_lifetime, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(invalid_config, audio_setup, audio_teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
