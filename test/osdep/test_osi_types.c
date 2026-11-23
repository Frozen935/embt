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
 * @brief OSI signatures and constants without backend layout assumptions.
 */

#include "support/test_support.h"

_Static_assert(sizeof(os_timeout_t) == sizeof(int64_t), "Millisecond timeout width");

/**
 * @brief Check interface signatures, storage declarations and timeout constants.
 * @param[in,out] state Unused cmocka state.
 */
static void test_types_constants(void **state)
{
	os_thread_t thread = OS_OBJECT_STORAGE_INIT;
	int (*create)(os_thread_t *, os_thread_entry_t, void *, const char *, int, size_t) =
		osi_thread_create;
	int (*join)(os_thread_t *) = osi_thread_join;
	os_thread_t (*self)(void) = osi_thread_self;
	bool (*equal)(os_thread_t, os_thread_t) = osi_thread_equal;
	bool (*is_current)(const os_thread_t *) = osi_thread_is_current;
	void (*fatal)(const char *) = osi_fatal;

	(void)state;
	(void)thread;
	assert_non_null(create);
	assert_non_null(join);
	assert_non_null(self);
	assert_non_null(equal);
	assert_non_null(is_current);
	assert_non_null(fatal);
	assert_int_equal(OS_TIMEOUT_NO_WAIT, 0);
	assert_int_equal(OS_TIMEOUT_FOREVER, -1);
}

/**
 * @brief Run registered OSI contracts without platform-specific process operations.
 * @param[in] argc Argument count; no selection runs all cases.
 * @param[in] argv Optional exact case selection.
 * @return Zero when all selected cases actually ran and passed.
 */
int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_types_constants),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
