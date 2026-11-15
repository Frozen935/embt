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

/** @file @brief Single assertion reporter, output failures and standard formatting. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <embt/base/assert.h>
#include <common/assert.h>
#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/uuid.h>
#include <embt/base/log.h>
#include <embt/osi/io.h>

LOG_MODULE_REGISTER(diagnostic_test, LOG_LEVEL_DBG);

void __real_osi_vprint(const char *, va_list);
static bool fail_sink;
static bool fail_allocation;
static unsigned int writes;
static unsigned int allocation_calls;

/**
 * @brief Observe diagnostic output through the OSI boundary.
 * @param[in] fmt Format string supplied by the diagnostic provider.
 * @param[in,out] args Formatting arguments.
 */
void __wrap_osi_vprint(const char *fmt, va_list args)
{
	writes++;
	if (!fail_sink) {
		__real_osi_vprint(fmt, args);
	}
}

void *__wrap_osi_malloc(size_t size)
{
	(void)size;
	allocation_calls++;
	if (fail_allocation) {
		return NULL;
	}
	abort();
}

static void test_assert_evaluation(void **state)
{
	int checked = 0;
	int formatted = 0;
	(void)state;
	__ASSERT(++checked == 1, "never format %d", ++formatted);
	assert_int_equal(checked, IS_ENABLED(CONFIG_ASSERT) ? 1 : 0);
	assert_int_equal(formatted, 0);
	checked = 0;
	BT_ASSERT_MSG(++checked == 1, "never format %d", ++formatted);
	assert_int_equal(checked, IS_ENABLED(CONFIG_ASSERT) || IS_ENABLED(CONFIG_BT_ASSERT) ? 1 : 0);
	assert_int_equal(formatted, 0);
}

static void assertion_child(int mode)
{
	struct rlimit limit = {0, 0};
	const char *message = "assert message %d";
	(void)setrlimit(RLIMIT_CORE, &limit);
	bt_log_set_level(LOG_LEVEL_NONE);
	if (mode == 0) {
		__ASSERT(0, message, 42);
	} else if (mode == 1) {
		BT_ASSERT_MSG(0, message, 42);
	} else if (mode == 2) {
		BT_ASSERT_CHECK(0, message, 42);
	} else if (mode == 3) {
		fail_sink = true;
		BT_ASSERT_CHECK(0, message, 42);
	} else {
		BT_ASSERT_CHECK(0);
	}
	(void)message;
	_exit(77);
}

static void check_assertion(int mode, bool enabled)
{
	int pipes[2], status;
	char output[1024] = {0};
	size_t size = 0;
	pid_t child;

	assert_int_equal(pipe(pipes), 0);
	child = fork();
	assert_true(child >= 0);
	if (child == 0) {
		dup2(pipes[1], STDERR_FILENO);
		close(pipes[0]);
		close(pipes[1]);
		assertion_child(mode);
	}
	close(pipes[1]);
	while (size < sizeof(output) - 1) {
		ssize_t got = read(pipes[0], output + size, sizeof(output) - 1 - size);
		if (got < 0 && errno == EINTR) {
			continue;
		}
		if (got <= 0) {
			break;
		}
		size += (size_t)got;
	}
	close(pipes[0]);
	assert_int_equal(waitpid(child, &status, 0), child);
	if (!enabled) {
		assert_true(WIFEXITED(status));
		assert_int_equal(WEXITSTATUS(status), 77);
		assert_string_equal(output, "");
		return;
	}
	assert_true(WIFSIGNALED(status));
	assert_int_equal(WTERMSIG(status), SIGABRT);
	if (mode == 1 && IS_ENABLED(CONFIG_BT_ASSERT) && !IS_ENABLED(CONFIG_BT_ASSERT_VERBOSE)) {
		assert_string_equal(output, "ASSERTION FAIL\n");
	} else if (mode != 3) {
		assert_non_null(strstr(output, "ASSERTION FAIL [0] @ "));
		assert_non_null(strstr(output, "test_diagnostics.c:"));
		if (mode != 4) {
			assert_non_null(strstr(output, "assert message 42"));
		}
	}
}
static void test_assert_general(void **state)
{
	(void)state;
	check_assertion(0, IS_ENABLED(CONFIG_ASSERT));
}
static void test_assert_bluetooth(void **state)
{
	(void)state;
	check_assertion(1, IS_ENABLED(CONFIG_ASSERT) || IS_ENABLED(CONFIG_BT_ASSERT));
}
static void test_assert_always(void **state)
{
	(void)state;
	check_assertion(2, true);
	check_assertion(4, true);
}
static void test_assert_output_failure(void **state)
{
	(void)state;
	check_assertion(3, true);
}

static char *capture_log(bool failing)
{
	char message[2048];
	FILE *file = tmpfile();
	int saved;
	long size;
	char *result;

	assert_non_null(file);
	saved = dup(STDERR_FILENO);
	assert_true(saved >= 0);
	fflush(stderr);
	dup2(fileno(file), STDERR_FILENO);
	memset(message, 'x', sizeof(message) - 1);
	message[sizeof(message) - 1] = '\0';
	fail_allocation = failing;
	writes = allocation_calls = 0;
	bt_log_set_level(LOG_LEVEL_DBG);
	LOG_ERR("long=%s", message);
	fail_allocation = false;
	dup2(saved, STDERR_FILENO);
	close(saved);
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	assert_true(size >= 0);
	result = malloc((size_t)size + 1);
	assert_non_null(result);
	rewind(file);
	assert_int_equal(fread(result, 1, (size_t)size, file), (size_t)size);
	result[size] = '\0';
	fclose(file);
	assert_int_equal(allocation_calls, 0);
	assert_int_equal(writes > 0, IS_ENABLED(CONFIG_EMBT_LOG));
	return result;
}
static void test_log_without_allocation(void **state)
{
	char *text;
	(void)state;
	text = capture_log(true);
	if (IS_ENABLED(CONFIG_EMBT_LOG)) {
		assert_non_null(strstr(text, "long=xxx"));
		assert_true(strlen(text) > 2048);
		assert_null(strstr(text, "<truncated>"));
	} else {
		assert_string_equal(text, "");
	}
	free(text);
}
static void test_log_sink_failure(void **state)
{
	(void)state;
	fail_sink = true;
	bt_log_set_level(LOG_LEVEL_DBG);
	LOG_ERR("sink failure must not terminate the protocol");
	fail_sink = false;
}
static void test_standard_formatting(void **state)
{
	char text[4];
	volatile size_t capacity = sizeof(text);
	char addr_text[BT_ADDR_STR_LEN];
	char uuid_text[BT_UUID_STR_LEN];
	bt_addr_t addr = {{0x66, 0x55, 0x44, 0x33, 0x22, 0x11}};
	struct bt_uuid_16 uuid = BT_UUID_INIT_16(0x180d);
	(void)state;
	assert_int_equal(snprintf(text, capacity, "%s", "abcdef"), 6);
	assert_string_equal(text, "abc");
	assert_int_equal(bt_addr_to_str(&addr, addr_text, sizeof(addr_text)), 17);
	assert_string_equal(addr_text, "11:22:33:44:55:66");
	bt_uuid_to_str(&uuid.uuid, uuid_text, sizeof(uuid_text));
	assert_string_equal(uuid_text, "180d");
}
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_assert_evaluation),
		cmocka_unit_test(test_assert_general),
		cmocka_unit_test(test_assert_bluetooth),
		cmocka_unit_test(test_assert_always),
		cmocka_unit_test(test_assert_output_failure),
		cmocka_unit_test(test_log_without_allocation),
		cmocka_unit_test(test_log_sink_failure),
		cmocka_unit_test(test_standard_formatting),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
