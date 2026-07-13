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

/** @file @brief Verify real codec admission and unsigned shell ID parsing. */
#include "support/test_support.h"
#include <errno.h>
#include <limits.h>
#include <embt/bluetooth/audio/lc3.h>
#include <shell/bt_shell.h>
#include "audio/shell/audio.h"

/** @brief The capability counts frames, including every channel in a block. */
static void codec_frame_limit(void **state)
{
	const struct bt_audio_codec_cap cap = BT_AUDIO_CODEC_CAP_LC3(
		BT_AUDIO_CODEC_CAP_FREQ_48KHZ, BT_AUDIO_CODEC_CAP_DURATION_ANY,
		BT_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT(1U, 2U), 30U, 155U, 4U,
		BT_AUDIO_CONTEXT_TYPE_MEDIA);
	const enum bt_audio_location stereo = BT_AUDIO_LOCATION_FRONT_LEFT |
		BT_AUDIO_LOCATION_FRONT_RIGHT;
	const struct bt_audio_codec_cfg oversized = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT | BT_AUDIO_LOCATION_FRONT_RIGHT, 30U, 3U,
		BT_AUDIO_CONTEXT_TYPE_MEDIA);
	const struct bt_audio_codec_cfg full = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT | BT_AUDIO_LOCATION_FRONT_RIGHT, 30U, 2U,
		BT_AUDIO_CONTEXT_TYPE_MEDIA);
	const struct bt_audio_codec_cfg mono = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_48KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_MONO_AUDIO, 30U, 4U, BT_AUDIO_CONTEXT_TYPE_MEDIA);

	(void)state;
	assert_false(supported_lc3_config(&cap, stereo, &oversized));
	assert_true(supported_lc3_config(&cap, stereo, &full));
	assert_true(supported_lc3_config(&cap, stereo, &mono));
}

static const struct CMUnitTest cases[] = {cmocka_unit_test(codec_frame_limit)};
BT_TEST_SUITE(shell_validation, cases, bt_test_base_setup, NULL);
