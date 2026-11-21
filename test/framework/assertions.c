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

/** @file @brief Main-thread assertions and isolated worker failures. */
#include "test.h"
#include <embt/osi/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static os_thread_t control_thread;

void bt_test_runtime_initialize(void)
{
	control_thread = osi_thread_self();
}

/* Cmocka may only longjmp from its controlling thread. A callback infrastructure
 * failure terminates the isolated case process instead of jumping across threads.
 */
static void callback_failure(const char *expression, const char *file, int line)
{
	fprintf(stderr, "Callback assertion failed at %s:%d: %s\n", file, line, expression);
	fflush(stderr);
	_Exit(1);
}
void __real__assert_true(const LargestIntegralType result, const char *expression,
			 const char *file, const int line);
void __wrap__assert_true(const LargestIntegralType result, const char *expression,
			 const char *file, const int line)
{
	if (osi_thread_equal(control_thread, osi_thread_self())) {
		__real__assert_true(result, expression, file, line);
	} else if (!result) {
		callback_failure(expression, file, line);
	}
}
void __real__assert_int_equal(const LargestIntegralType a, const LargestIntegralType b,
			      const char *file, const int line);
void __wrap__assert_int_equal(const LargestIntegralType a, const LargestIntegralType b,
			      const char *file, const int line)
{
	if (osi_thread_equal(control_thread, osi_thread_self())) {
		__real__assert_int_equal(a, b, file, line);
	} else if (a != b) {
		callback_failure("integer equality", file, line);
	}
}

bool bt_test_is_control_thread(void)
{
 return osi_thread_equal(control_thread, osi_thread_self());
}

void __real__assert_int_not_equal(LargestIntegralType a, LargestIntegralType b,
                                 const char *file, int line);
void __wrap__assert_int_not_equal(LargestIntegralType a, LargestIntegralType b,
                                 const char *file, int line)
{
	if (bt_test_is_control_thread()) {
		__real__assert_int_not_equal(a, b, file, line);
	} else if (a == b) {
		callback_failure("integer inequality", file, line);
	}
}
void __real__assert_memory_equal(const void *a, const void *b, size_t size,
                                const char *file, int line);
void __wrap__assert_memory_equal(const void *a, const void *b, size_t size,
                                const char *file, int line)
{
	if (bt_test_is_control_thread()) {
		__real__assert_memory_equal(a, b, size, file, line);
	} else if (memcmp(a, b, size)) {
		callback_failure("memory equality", file, line);
	}
}
void __real__assert_string_equal(const char *a, const char *b, const char *file, int line);
void __wrap__assert_string_equal(const char *a, const char *b, const char *file, int line)
{
	if (bt_test_is_control_thread()) {
		__real__assert_string_equal(a, b, file, line);
	} else if (strcmp(a, b)) {
		callback_failure("string equality", file, line);
	}
}
void __real__fail(const char *file, int line);
void __wrap__fail(const char *file, int line)
{
	if (bt_test_is_control_thread()) {
		__real__fail(file, line);
	} else {
		callback_failure("explicit test failure", file, line);
	}
	abort();
}
