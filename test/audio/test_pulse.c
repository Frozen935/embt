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

/** @file @brief Native PulseAudio open, drain, drop, restart and failure integration. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <embt/drivers/audio.h>
#include <embt/osi/time.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/** @brief Wait on observable asynchronous state using a finite monotonic deadline. */
static void wait_state(struct bt_audio_stream *stream, enum bt_audio_state expected)
{
	struct bt_audio_status status;
	uint64_t start, now;
	osi_time_get_ms(&start);
	for (;;) {
		assert_int_equal(bt_audio_get_status(stream, &status), 0);
		if (status.state == expected) {
			return;
		}
		assert_int_not_equal(status.state, BT_AUDIO_ERROR);
		osi_time_get_ms(&now);
		assert_true(now - start < 12000);
		osi_sleep_ms(5);
	}
}

/** @brief Open two PCM streams, consume data, drain, restart, drop and close. */
static void playback(void **state)
{
	struct bt_audio_stream *stream, *second;
	struct bt_audio_status status;
	struct bt_audio_format format = {48000, 2};
	int16_t pcm[256];
	(void)state;
	for (size_t i = 0; i < 256; i++) {
		pcm[i] = ((int)(i % 24) - 12) * 800;
	}
	assert_int_equal(bt_audio_open("pulse", getenv("EMBT_AUDIO_DEVICE"), &format,
				      NULL, NULL, &stream), 0);
	assert_int_equal(bt_audio_open("pulse", getenv("EMBT_AUDIO_DEVICE"), &format,
				      NULL, NULL, &second), 0);
	wait_state(stream, BT_AUDIO_READY);
	wait_state(second, BT_AUDIO_READY);
	for (unsigned int i = 0; i < 12; i++) {
		assert_int_equal(bt_audio_submit(stream, pcm, 128), 0);
	}
	assert_int_equal(bt_audio_stop(stream, true), 0);
	wait_state(stream, BT_AUDIO_STOPPED);
	bt_audio_get_status(stream, &status);
	assert_int_equal(status.consumed_frames, 1536);
	assert_int_equal(status.queued_frames, 0);
	assert_int_equal(bt_audio_start(stream), 0);
	assert_int_equal(bt_audio_submit(stream, pcm, 128), 0);
	assert_int_equal(bt_audio_stop(stream, false), 0);
	wait_state(stream, BT_AUDIO_STOPPED);
	assert_int_equal(bt_audio_close(stream), 0);
	assert_int_equal(bt_audio_close(second), 0);
	wait_state(stream, BT_AUDIO_CLOSED);
	wait_state(second, BT_AUDIO_CLOSED);
}

/** @brief A nonexistent sink fails observably and the slot can subsequently be closed. */
static void invalid_device(void **state)
{
	struct bt_audio_stream *stream;
	struct bt_audio_status status;
	struct bt_audio_format format = {48000, 2};
	uint64_t start, now;
	(void)state;
	assert_int_equal(bt_audio_open("pulse", "embt_nonexistent_device_7654321", &format,
				      NULL, NULL, &stream), 0);
	osi_time_get_ms(&start);
	do {
		bt_audio_get_status(stream, &status);
		osi_time_get_ms(&now);
		assert_true(now - start < 12000);
		osi_sleep_ms(5);
	} while (status.state != BT_AUDIO_ERROR);
	assert_true(status.error < 0);
	assert_int_equal(bt_audio_close(stream), 0);
	wait_state(stream, BT_AUDIO_CLOSED);
}

/** @brief Refuse an unavailable server without spawning or selecting a fallback. */
static void unavailable_server(void **state)
{
	struct bt_audio_stream *stream;
	struct bt_audio_format format = {48000, 2};
	const char *previous = getenv("PULSE_SERVER");
	char *saved = previous ? strdup(previous) : NULL;
	(void)state;
	assert_int_equal(setenv("PULSE_SERVER", "unix:/tmp/embt-unavailable-pulse/socket", 1), 0);
	assert_int_equal(bt_audio_open("pulse", NULL, &format, NULL, NULL, &stream), 0);
	wait_state(stream, BT_AUDIO_ERROR);
	assert_int_equal(bt_audio_close(stream), 0);
	wait_state(stream, BT_AUDIO_CLOSED);
	if (saved) {
		setenv("PULSE_SERVER", saved, 1);
		free(saved);
	} else {
		unsetenv("PULSE_SERVER");
	}
}

/** @brief Record closure only; assertions remain on the test's controlling thread. */
static void closed_event(struct bt_audio_stream *stream, enum bt_audio_state state,
			 int error, void *user)
{
	(void)stream;
	(void)error;
	if (state == BT_AUDIO_CLOSED) {
		(*(int *)user)++;
	}
}

/** @brief Shutdown immediately after open and confirm close notifications precede return. */
static void opening_shutdown(void **state)
{
	struct bt_audio_stream *stream;
	struct bt_audio_format format = {48000, 2};
	int closed = 0;
	(void)state;
	assert_int_equal(bt_audio_open("pulse", getenv("EMBT_AUDIO_DEVICE"), &format,
				      closed_event, &closed, &stream), 0);
	bt_audio_driver_shutdown();
	assert_int_equal(closed, 1);
}

/** @brief Initialize a provider without connecting before open. */
static int setup(void **state)
{
	int err = bt_audio_driver_init();
	(void)state;
	return err ? err : bt_audio_pulse_register();
}

/** @brief Join the provider event loop and release all native resources. */
static int teardown(void **state)
{
	(void)state;
	bt_audio_driver_shutdown();
	return 0;
}

/** @brief Run against an explicitly selected test sink. */
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(playback, setup, teardown),
		cmocka_unit_test_setup_teardown(invalid_device, setup, teardown),
		cmocka_unit_test_setup_teardown(unavailable_server, setup, teardown),
		cmocka_unit_test_setup_teardown(opening_shutdown, setup, teardown),
	};
	if (!getenv("EMBT_AUDIO_DEVICE")) {
		return 2;
	}
	return cmocka_run_group_tests(tests, NULL, NULL);
}
