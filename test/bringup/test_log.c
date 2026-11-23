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

/**
 * @file
 * @brief Verify logging output, filtering and concurrent call-site admission.
 */

#include <embt/base/atomic.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <embt/osi/thread.h>
#include <embt/osi/sync.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <embt/base/log.h>
#include "test_log_modules.h"
#include "common/bt_str.h"

LOG_MODULE_REGISTER(log_test);

static FILE *capture;
static int saved_stderr;

static int setup(void **state)
{
	(void)state;
	capture = tmpfile();
	assert_non_null(capture);
	saved_stderr = dup(STDERR_FILENO);
	assert_true(saved_stderr >= 0);
	assert_int_equal(fflush(stderr), 0);
	assert_int_equal(dup2(fileno(capture), STDERR_FILENO), STDERR_FILENO);
	assert_int_equal(bt_log_set_level(LOG_LEVEL_DBG), 0);
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	fflush(stderr);
	dup2(saved_stderr, STDERR_FILENO);
	close(saved_stderr);
	fclose(capture);
	bt_log_set_level(LOG_LEVEL_DBG);
	return 0;
}

static char *captured(void)
{
	long size;
	char *text;

	assert_int_equal(fflush(stderr), 0);
	assert_int_equal(fseek(capture, 0, SEEK_END), 0);
	size = ftell(capture);
	assert_true(size >= 0);
	text = malloc((size_t)size + 1);
	assert_non_null(text);
	rewind(capture);
	assert_int_equal(fread(text, 1, (size_t)size, capture), (size_t)size);
	text[size] = '\0';
	return text;
}

static void test_filter(void **state)
{
	char *text;
	int evaluated = 0;

	(void)state;
	assert_int_equal(bt_log_get_level(), BT_LOG_LEVEL_RESOLVE(LOG_LEVEL_DBG));
	assert_false(bt_log_level_check(LOG_LEVEL_NONE));
	assert_false(bt_log_level_check(LOG_LEVEL_DBG + 1));
	assert_int_equal(bt_log_set_level(-1), -EINVAL);
	assert_int_equal(bt_log_set_level(LOG_LEVEL_DBG + 1), -EINVAL);
	assert_int_equal(bt_log_set_level(LOG_LEVEL_NONE), 0);
	LOG_ERR("suppressed %d", ++evaluated);
	LOG_HEXDUMP_ERR(NULL, ++evaluated, "suppressed dump");
	LOG_WRN_RATELIMIT("suppressed rate %d", ++evaluated);
	assert_int_equal(evaluated, 0);
	text = captured();
	assert_string_equal(text, "");
	free(text);
}

static void test_output(void **state)
{
	char long_message[2048];
	char *text;
	const unsigned char bytes[] = {0x00, 0x7f, 0xff};

	(void)state;
	memset(long_message, 'x', sizeof(long_message) - 1);
	long_message[sizeof(long_message) - 1] = '\0';
	LOG_ERR("message=%d", 42);
	LOG_WRN("warning");
	LOG_INF("information");
	LOG_DBG("debug");
	LOG_HEXDUMP_ERR(bytes, sizeof(bytes), "binary=%u", 3U);
	LOG_ERR("long=%s", long_message);
	text = captured();
	if (LOG_LEVEL >= LOG_LEVEL_ERR) {
		assert_non_null(strstr(text, "[E][log_test:"));
		assert_non_null(strstr(text, "test_output: message=42\n"));
		assert_non_null(strstr(text, "binary=3\n  0000: 00 7f ff\n"));
		assert_non_null(strstr(text, long_message));
	} else {
		assert_string_equal(text, "");
	}
	assert_int_equal(strstr(text, "warning\n") != NULL,
			 LOG_LEVEL >= LOG_LEVEL_WRN);
	assert_int_equal(strstr(text, "information\n") != NULL,
			 LOG_LEVEL >= LOG_LEVEL_INF);
	assert_int_equal(strstr(text, "debug\n") != NULL,
			 LOG_LEVEL >= LOG_LEVEL_DBG);
	free(text);
}

static void test_debug_dump(void **state)
{
	unsigned char data = 0xaa;
	int evaluated = 0;
	char *text;

	(void)state;
	LOG_HEXDUMP_DBG(&data, ++evaluated, "debug bytes");
	text = captured();
	if (LOG_LEVEL >= LOG_LEVEL_DBG) {
		assert_int_equal(evaluated, 1);
		assert_non_null(strstr(text, "debug bytes\n  0000: aa\n"));
	} else {
		assert_int_equal(evaluated, 0);
		assert_string_equal(text, "");
	}
	free(text);
}

static void test_ratelimit(void **state)
{
	bt_atomic_t last = 0;
	char *text;
	int evaluated = 0;

	(void)state;
	assert_true(bt_log_ratelimit(&last, 1000));
	assert_false(bt_log_ratelimit(&last, 1000));
	assert_false(bt_log_ratelimit(&last, UINT32_MAX));
	assert_true(bt_log_ratelimit(&last, 0));
	for (int i = 0; i < 3; i++) {
		LOG_ERR_RATELIMIT_RATE(1000, "rate=%d", ++evaluated);
	}
	text = captured();
	assert_int_equal(evaluated, LOG_LEVEL >= LOG_LEVEL_ERR);
	if (LOG_LEVEL >= LOG_LEVEL_ERR) {
		assert_non_null(strstr(text, "rate=1\n"));
		assert_null(strstr(text, "rate=2\n"));
	}
	free(text);
}

static void writer(void *arg)
{
	size_t id = (size_t)arg;

	for (int i = 0; i < 50; i++) {
		LOG_ERR("worker=%zu item=%d payload=abcdefghijklmnop", id, i);
	}
}

static void test_concurrent_output(void **state)
{
	os_thread_t threads[4];
	bool seen[4][50] = {{false}};
	char *text;
	char *save;
	char *line;
	int count = 0;

	(void)state;
	for (size_t i = 0; i < 4; i++) {
		assert_int_equal(osi_thread_create(&threads[i], writer, (void *)i, "log-writer", 0, 0), 0);
	}
	for (size_t i = 0; i < 4; i++) {
		assert_int_equal(osi_thread_join(&threads[i]), 0);
	}
	text = captured();
	line = strtok_r(text, "\n", &save);
	while (line) {
		char *message = strstr(line, "writer: worker=");
		size_t id;
		int item;
		int length = 0;

		assert_non_null(message);
		assert_int_equal(sscanf(message, "writer: worker=%zu item=%d payload=abcdefghijklmnop%n",
					&id, &item, &length), 2);
		assert_int_equal(message[length], '\0');
		assert_true(id < 4 && item >= 0 && item < 50);
		assert_false(seen[id][item]);
		seen[id][item] = true;
		count++;
		line = strtok_r(NULL, "\n", &save);
	}
	assert_int_equal(count, LOG_LEVEL >= LOG_LEVEL_ERR ? 200 : 0);
	free(text);
}

struct formatting_context {
	os_sem_t *ready;
	os_sem_t *release;
	unsigned char value;
	bool valid;
};

/**
 * @brief Hold thread-local formatting results until both OSI workers have produced them.
 * @param[in,out] argument Context retained until join.
 */
static void formatting_writer(void *argument)
{
	struct formatting_context *context = argument;
	struct bt_uuid_16 uuid = BT_UUID_INIT_16(context->value);
	char expected_uuid[BT_UUID_STR_LEN];
	const char *hex = bt_hex(&context->value, 1);
	const char *uuid_text = bt_uuid_str(&uuid.uuid);
	const char *expected_hex = context->value == 1 ? "01" : "02";

	bt_uuid_to_str(&uuid.uuid, expected_uuid, sizeof(expected_uuid));
	if (osi_sem_give(context->ready) || osi_sem_take(context->release, 2000)) {
		return;
	}
	context->valid = !strcmp(hex, expected_hex) && !strcmp(uuid_text, expected_uuid);
}

static void test_concurrent_formatting(void **state)
{
	os_thread_t threads[2];
	os_sem_t ready, release;
	struct formatting_context contexts[2];

	(void)state;
	assert_int_equal(osi_sem_init(&ready, 0, 2), 0);
	assert_int_equal(osi_sem_init(&release, 0, 2), 0);
	for (size_t i = 0; i < 2; i++) {
		contexts[i] = (struct formatting_context){&ready, &release, (unsigned char)(i + 1), false};
		assert_int_equal(osi_thread_create(&threads[i], formatting_writer, &contexts[i],
					 "format-test", 0, 0), 0);
	}
	for (size_t i = 0; i < 2; i++) {
		assert_int_equal(osi_sem_take(&ready, 2000), 0);
	}
	for (size_t i = 0; i < 2; i++) {
		assert_int_equal(osi_sem_give(&release), 0);
	}
	for (size_t i = 0; i < 2; i++) {
		assert_int_equal(osi_thread_join(&threads[i]), 0);
		assert_true(contexts[i].valid);
	}
	assert_int_equal(osi_sem_deinit(&ready), 0);
	assert_int_equal(osi_sem_deinit(&release), 0);
}

static void test_module_levels(void **state)
{
	char *text;
	bool enabled = BT_LOG_LEVEL_RESOLVE(LOG_LEVEL_DBG) == LOG_LEVEL_DBG;

	(void)state;
	assert_int_equal(test_log_debug_emit(), enabled ? 3 : 0);
	assert_int_equal(test_log_declared_emit(), enabled ? 1 : 0);
	assert_int_equal(test_log_warn_emit(), enabled ? 1 : 0);
	assert_int_equal(test_log_off_emit(), 0);
	text = captured();
	if (enabled) {
		assert_non_null(strstr(text, "[D][log_debug:"));
		assert_non_null(strstr(text, "module debug 1\n"));
		assert_non_null(strstr(text, "module dump\n  0000: aa bb\n"));
		assert_non_null(strstr(text, "module rate 3\n"));
		assert_non_null(strstr(text, "declared debug 1\n"));
		assert_non_null(strstr(text, "[W][log_warn:"));
	} else {
		assert_string_equal(text, "");
	}
	assert_null(strstr(text, "suppressed module"));
	free(text);
}

static void test_module_runtime_filter(void **state)
{
	char *text;
	bool enabled = BT_LOG_LEVEL_RESOLVE(LOG_LEVEL_DBG) == LOG_LEVEL_DBG;
	const bool conn_debug = BT_LOG_LEVEL_ENABLED(BT_CONN, DBG);

	(void)state;
	assert_int_equal(bt_log_set_level(LOG_LEVEL_WRN), 0);
	assert_int_equal(BT_LOG_LEVEL_ENABLED(BT_CONN, DBG), conn_debug);
	assert_int_equal(test_log_debug_emit(), 0);
	assert_int_equal(test_log_declared_emit(), 0);
	assert_int_equal(test_log_warn_emit(), enabled ? 1 : 0);
	assert_int_equal(test_log_off_emit(), 0);
	text = captured();
	assert_null(strstr(text, "module debug"));
	assert_null(strstr(text, "module dump"));
	assert_null(strstr(text, "module rate"));
	assert_null(strstr(text, "suppressed module"));
	assert_int_equal(strstr(text, "module warning") != NULL, enabled);
	free(text);
	assert_int_equal(bt_log_set_level(LOG_LEVEL_DBG), 0);
	assert_int_equal(BT_LOG_LEVEL_ENABLED(BT_CONN, DBG), conn_debug);
	assert_int_equal(test_log_debug_emit(), enabled ? 3 : 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_filter, setup, teardown),
		cmocka_unit_test_setup_teardown(test_output, setup, teardown),
		cmocka_unit_test_setup_teardown(test_debug_dump, setup, teardown),
		cmocka_unit_test_setup_teardown(test_ratelimit, setup, teardown),
		cmocka_unit_test_setup_teardown(test_concurrent_output, setup, teardown),
		cmocka_unit_test_setup_teardown(test_concurrent_formatting, setup, teardown),
		cmocka_unit_test_setup_teardown(test_module_levels, setup, teardown),
		cmocka_unit_test_setup_teardown(test_module_runtime_filter, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
