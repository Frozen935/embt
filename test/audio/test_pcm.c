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

/** @file @brief PCM registry, ownership, queue pressure and asynchronous lifecycle tests. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "fake.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

/** @brief Verify duplicate registration, capacity and unknown-provider errors. */
static void registry(void **state)
{
	static const struct bt_audio_driver extra[] = {
		{"third", fake_pump, NULL},
	};
	struct bt_audio_stream *stream;
	struct bt_audio_format format = {48000, 2};
	static struct bt_audio_driver extra_drivers[CONFIG_EMBT_AUDIO_DRIVERS];
	static char names[CONFIG_EMBT_AUDIO_DRIVERS][16];
	(void)state;
	assert_int_equal(bt_audio_driver_register(&fake_driver), -EEXIST);
	assert_int_equal(bt_audio_driver_register(&extra[0]), -EINVAL);
	for (size_t i = 2; i < CONFIG_EMBT_AUDIO_DRIVERS; i++) {
		snprintf(names[i], sizeof(names[i]), "extra_%zu", i);
		extra_drivers[i] = fake_driver;
		extra_drivers[i].name = names[i];
		assert_int_equal(bt_audio_driver_register(&extra_drivers[i]), 0);
	}
	extra_drivers[0] = fake_driver;
	extra_drivers[0].name = "overflow";
	assert_int_equal(bt_audio_driver_register(&extra_drivers[0]), -ENOSPC);
	assert_int_equal(bt_audio_open("missing", NULL, &format, NULL, NULL, &stream), -ENODEV);
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &stream), 0);
	assert_int_equal(bt_audio_driver_register(&fake_driver), -EBUSY);
}

/** @brief Verify caller-buffer copying, partial consumption and bounded whole-block acceptance. */
static void queue(void **state)
{
	struct bt_audio_stream *stream;
	struct bt_audio_status status;
	struct bt_audio_format format = {48000, 2};
	int16_t pcm[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	int16_t copy[8];
	(void)state;
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &stream), 0);
	assert_int_equal(bt_audio_submit(stream, pcm, 0), -EINVAL);
	assert_int_equal(bt_audio_submit(stream, pcm, CONFIG_EMBT_AUDIO_BLOCK_BYTES), -EMSGSIZE);
	fake_hold = true;
	assert_int_equal(bt_audio_submit(stream, pcm, 4), 0);
	memset(pcm, 0, sizeof(pcm));
	assert_int_equal(bt_audio_peek(stream, copy, 8), 8);
	assert_int_equal(copy[0], 1);
	assert_int_equal(copy[3], 4);
	bt_audio_consume(stream, 8);
	assert_int_equal(bt_audio_peek(stream, copy, sizeof(copy)), 8);
	assert_int_equal(copy[0], 5);
	for (size_t i = 1; i < CONFIG_EMBT_AUDIO_QUEUE_BLOCKS; i++) {
		assert_int_equal(bt_audio_submit(stream, pcm, 4), 0);
	}
	assert_int_equal(bt_audio_submit(stream, pcm, 4), -EAGAIN);
	bt_audio_get_status(stream, &status);
	assert_int_equal(status.submitted_frames, 4 * CONFIG_EMBT_AUDIO_QUEUE_BLOCKS);
	assert_int_equal(status.consumed_frames, 2);
	fake_hold = false;
	fake_pump();
	bt_audio_get_status(stream, &status);
	assert_int_equal(status.queued_frames, 0);
	assert_int_equal(status.consumed_frames, status.submitted_frames);
}

/** @brief Verify independent backend/device slots and finite slot exhaustion. */
static void isolation(void **state)
{
	struct bt_audio_stream *streams[CONFIG_EMBT_AUDIO_STREAMS], *extra;
	struct bt_audio_format format = {44100, 1};
	(void)state;
	for (size_t i = 0; i < CONFIG_EMBT_AUDIO_STREAMS; i++) {
		assert_int_equal(bt_audio_open(i % 2 ? "other" : "memory", i % 2 ? "B" : "A",
					      &format, NULL, NULL, &streams[i]), 0);
		assert_string_equal(streams[i]->device, i % 2 ? "B" : "A");
	}
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &extra), -ENOSPC);
	assert_int_equal(bt_audio_close(streams[0]), 0);
	assert_int_equal(bt_audio_open("other", NULL, &format, NULL, NULL, &extra), 0);
}

/** @brief Verify drop/drain distinction, restart and rejection after stopping. */
static void lifecycle(void **state)
{
	struct bt_audio_stream *stream;
	struct bt_audio_status status;
	struct bt_audio_format format = {48000, 2};
	int16_t pcm[8] = {0};
	(void)state;
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &stream), 0);
	fake_hold = true;
	assert_int_equal(bt_audio_submit(stream, pcm, 4), 0);
	assert_int_equal(bt_audio_stop(stream, false), 0);
	assert_int_equal(bt_audio_submit(stream, pcm, 4), -ESHUTDOWN);
	fake_hold = false;
	fake_pump();
	assert_int_equal(fake_length, 0);
	assert_int_equal(bt_audio_start(stream), 0);
	fake_hold = true;
	assert_int_equal(bt_audio_submit(stream, pcm, 4), 0);
	assert_int_equal(bt_audio_stop(stream, true), 0);
	fake_hold = false;
	fake_pump();
	bt_audio_get_status(stream, &status);
	assert_int_equal(status.state, BT_AUDIO_STOPPED);
	assert_int_equal(status.consumed_frames, 4);
	assert_int_equal(fake_length, sizeof(pcm));
}

/** @brief Close before asynchronous readiness and preserve native provider errors. */
static void failures(void **state)
{
	struct bt_audio_stream *stream;
	struct bt_audio_status status;
	struct bt_audio_format format = {48000, 2};
	int16_t pcm[2] = {0};
	(void)state;
	fake_hold = true;
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &stream), 0);
	assert_int_equal(bt_audio_submit(stream, pcm, 1), -EAGAIN);
	assert_int_equal(bt_audio_close(stream), 0);
	bt_audio_complete(stream, BT_AUDIO_READY, 0);
	fake_hold = false;
	fake_pump();
	bt_audio_get_status(stream, &status);
	assert_int_equal(status.state, BT_AUDIO_CLOSED);
	fake_error = -ENODEV;
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &stream), 0);
	bt_audio_get_status(stream, &status);
	assert_int_equal(status.state, BT_AUDIO_ERROR);
	assert_int_equal(status.error, -ENODEV);
	assert_int_equal(bt_audio_submit(stream, pcm, 1), -ESHUTDOWN);
	assert_int_equal(bt_audio_close(stream), 0);
	format.channels = 3;
	assert_int_equal(bt_audio_open("memory", NULL, &format, NULL, NULL, &stream), -EINVAL);
}

/** @brief Run independently isolated PCM contract cases. */
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(registry, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(queue, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(isolation, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(lifecycle, audio_setup, audio_teardown),
		cmocka_unit_test_setup_teardown(failures, audio_setup, audio_teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
