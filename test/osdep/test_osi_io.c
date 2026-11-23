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
 * @brief OSI byte-write results and formatted-print invocation contracts.
 */

#include "support/test_support.h"

/**
 * @brief Validate byte counts and invalid output arguments without a platform capture device.
 * @param[in,out] state Unused cmocka state.
 */
static void test_io_bytes(void **state)
{
	const char payload[] = {'a', '\0', 'b', '\n'};
	size_t written = 99;

	(void)state;
	assert_int_equal(osi_console_write(payload, sizeof(payload), &written), 0);
	assert_int_equal(written, sizeof(payload));
	assert_int_equal(osi_console_write(NULL, 0, &written), 0);
	assert_int_equal(written, 0);
	assert_int_equal(osi_console_write(NULL, 1, &written), -EINVAL);
	assert_int_equal(written, 0);
	assert_int_equal(osi_console_write(payload, 1, NULL), -EINVAL);
}

/**
 * @brief Forward a valid variadic invocation to the OSI formatting interface.
 * @param[in] format Valid format string.
 * @note Actual sink contents cannot be inspected through the existing OSI interface.
 */
static void print_text(const char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	osi_vprint(format, arguments);
	va_end(arguments);
}

/**
 * @brief Exercise valid formatting calls without assuming stderr, syslog or a capture backend.
 * @param[in,out] state Unused cmocka state.
 */
static void test_io_vprint(void **state)
{
	(void)state;
	print_text("OSI print contract: item=%d %s %%\n", 42, "ready");
	print_text("%s", "");
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
		cmocka_unit_test(test_io_bytes),
		cmocka_unit_test(test_io_vprint),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
