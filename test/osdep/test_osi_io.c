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
 * @brief Raw byte output and fork-exec isolated fatal behavior tests.
 */

#include "support/test_support.h"

#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <string.h>
/**
 * @brief Verify io bytes behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_io_bytes(void **state)
{
	const char payload[] = {'a', '\0', 'b', '\n'};
	char received[8];
	int descriptors[2], saved;
	size_t written;
	int rc;
	ssize_t count;
	(void)state;
	assert_int_equal(pipe(descriptors), 0);
	saved = dup(STDERR_FILENO);
	dup2(descriptors[1], STDERR_FILENO);
	rc = osi_console_write(payload, sizeof(payload), &written);
	dup2(saved, STDERR_FILENO);
	close(saved);
	close(descriptors[1]);
	count = read(descriptors[0], received, sizeof(received));
	close(descriptors[0]);
	assert_int_equal(rc, 0);
	assert_int_equal(written, sizeof(payload));
	assert_int_equal(count, sizeof(payload));
	assert_memory_equal(received, payload, sizeof(payload));
	written = 99;
	assert_int_equal(osi_console_write(NULL, 0, &written), 0);
	assert_int_equal(written, 0);
	assert_int_equal(osi_console_write(NULL, 1, &written), -EINVAL);
	assert_int_equal(written, 0);
	assert_int_equal(osi_console_write(payload, 1, NULL), -EINVAL);
}
/**
 * @brief Execute fatal in a fresh process and inspect its termination.
 * @param[in] mode Child execution mode.
 * @param[in] with_message Whether a diagnostic payload is expected.
 */
static void run_fatal(const char *mode, bool with_message)
{
	int descriptors[2], status;
	char data[64] = {0};
	pid_t child;
	assert_int_equal(pipe(descriptors), 0);
	child = fork();
	if (child < 0) {
		_exit(1);
	}
	if (child == 0) {
		dup2(descriptors[1], STDERR_FILENO);
		close(descriptors[0]);
		close(descriptors[1]);
		execl("/proc/self/exe", "test_osi_io", mode, (char *)NULL);
		_exit(127);
	}
	close(descriptors[1]);
	ssize_t count = 0, received;
	/* Drain to EOF before closing: the child may still write its trailing newline. */
	while (count < (ssize_t)sizeof(data) - 1) {
		received = read(descriptors[0], data + count, sizeof(data) - 1 - (size_t)count);
		if (received < 0 && errno == EINTR) {
			continue;
		}
		if (received <= 0) {
			break;
		}
		count += received;
	}
	close(descriptors[0]);
	waitpid(child, &status, 0);
	assert_true(child > 0);
	assert_true(WIFSIGNALED(status));
	assert_int_equal(WTERMSIG(status), SIGABRT);
	if (with_message) {
		assert_true(count > 0);
		assert_non_null(strstr(data, "fatal marker"));
	}
}
/**
 * @brief Verify io fatal message behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_io_fatal_message(void **state)
{
	(void)state;
	run_fatal("--fatal-message", true);
}
/**
 * @brief Verify io fatal null behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_io_fatal_null(void **state)
{
	(void)state;
	run_fatal("--fatal-null", false);
}

/**
 * @brief Run one registered case in its own process.
 * @param[in] argc Argument count.
 * @param[in] argv Arguments containing the exact case name.
 * @return Test result or a selection error.
 */
int main(int argc, char **argv)
{
	if (argc == 2 && strncmp(argv[1], "--fatal-", 8) == 0) {
		struct rlimit limit = {0, 0};
		setrlimit(RLIMIT_CORE, &limit);
		osi_fatal(strcmp(argv[1], "--fatal-null") == 0 ? NULL : "fatal marker");
	}
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_io_bytes),
		cmocka_unit_test(test_io_fatal_message),
		cmocka_unit_test(test_io_fatal_null),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
