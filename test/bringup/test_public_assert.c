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

/** @file @brief Public utilities keep assertion checks without private base headers. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <embt/base/ring_buffer.h>

static void test_capacity_check(void **state)
{
	pid_t child;
	int status;
	(void)state;
	child = fork();
	assert_true(child >= 0);
	if (child == 0) {
		struct rlimit limit = {0, 0};
		struct ring_buf rb;
		unsigned char data[8];
		(void)setrlimit(RLIMIT_CORE, &limit);
		ring_buf_init(&rb, RING_BUFFER_MAX_SIZE + 1U, data);
		_exit(77);
	}
	assert_int_equal(waitpid(child, &status, 0), child);
	if (IS_ENABLED(CONFIG_ASSERT)) {
		assert_true(WIFSIGNALED(status));
		assert_int_equal(WTERMSIG(status), SIGABRT);
	} else {
		assert_true(WIFEXITED(status));
		assert_int_equal(WEXITSTATUS(status), 77);
	}
}
int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(test_capacity_check)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
