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
 * @brief OSI native type, constant and signature checks.
 */

#include "support/test_support.h"

_Static_assert(sizeof(os_thread_t) == sizeof(pthread_t), "Minimal thread handle");
_Static_assert(sizeof(os_mutex_t) == sizeof(pthread_mutex_t), "Native mutex");
_Static_assert(sizeof(os_cond_t) == sizeof(pthread_cond_t), "Native condition");
_Static_assert(sizeof(os_timeout_t) == 8, "64-bit millisecond timeout");
/**
 * @brief Verify types constants behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_types_constants(void **state)
{
	os_thread_t storage = OS_OBJECT_STORAGE_INIT;
	int (*create)(os_thread_t *, void (*)(void *), void *, const char *, int, size_t) =
		osi_thread_create;
	int (*start)(os_thread_t *) = osi_thread_start;
	int (*cancel)(os_thread_t *) = osi_thread_cancel;
	int (*join)(os_thread_t *) = osi_thread_join;
	(void)state;
	(void)storage;
	assert_non_null(create);
	assert_non_null(start);
	assert_non_null(cancel);
	assert_non_null(join);
	assert_int_equal(OS_TIMEOUT_NO_WAIT, 0);
	assert_int_equal(OS_TIMEOUT_FOREVER, -1);
}

/**
 * @brief Run one registered case in its own process.
 * @param[in] argc Argument count.
 * @param[in] argv Arguments containing the exact case name.
 * @return Test result or a selection error.
 */
int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_types_constants),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
