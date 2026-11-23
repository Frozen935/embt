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

/** @file @brief Cmocka interception of expected Bluetooth invariant failures. */
#include "test_support.h"
#include <embt/base/assert.h>
#include <setjmp.h>
static _Thread_local bool expected_thread_fault;
static _Thread_local jmp_buf expected_assertion;

bool bt_test_thread_assert(os_thread_entry_t entry, void *argument)
{
	if (expected_thread_fault) {
		abort();
	}
	expected_thread_fault = true;
	if (setjmp(expected_assertion) == 0) {
		entry(argument);
		expected_thread_fault = false;
		return false;
	}
	expected_thread_fault = false;
	return true;
}

FUNC_NORETURN void __wrap_bt_assert_fail(const char *condition, const char *file,
 unsigned int line, const char *fmt, ...)
{
 (void)fmt;
 if (expected_thread_fault) { longjmp(expected_assertion, 1); }
 if (!bt_test_is_control_thread()) {
  fprintf(stderr, "Unexpected assertion from a callback thread\n");
  _Exit(1);
 }
 mock_assert(0, condition ? condition : "Bluetooth invariant", file ? file : __FILE__, line);
 abort();
}
