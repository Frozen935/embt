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
 * @brief Monotonic clock, checked unit conversion and finite sleep tests.
 */

#include "support/test_support.h"

#include <limits.h>
/**
 * @brief Verify time units behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_time_units(void **state)
{
	const int64_t input[] = {0, 1, 999, 1000, 1001, INT64_MAX};
	os_timeout_t value;
	(void)state;
	for (size_t i = 0; i < TEST_ARRAY_SIZE(input); i++) {
		assert_int_equal(osi_timeout_from_us(input[i], &value), 0);
		assert_true(value == input[i] / 1000 + (input[i] % 1000 != 0));
	}
	assert_int_equal(osi_timeout_from_seconds(INT64_MAX / 1000, &value), 0);
	assert_true(value == (INT64_MAX / 1000) * 1000);
	value = 71;
	assert_int_equal(osi_timeout_from_seconds(INT64_MAX / 1000 + 1, &value), -EOVERFLOW);
	assert_int_equal(value, 71);
	assert_int_equal(osi_timeout_from_us(-1, &value), -EINVAL);
	assert_int_equal(value, 71);
	assert_int_equal(osi_timeout_from_seconds(-1, &value), -EINVAL);
	assert_int_equal(osi_timeout_from_us(1, NULL), -EINVAL);
	assert_int_equal(osi_timeout_from_seconds(1, NULL), -EINVAL);
}
/**
 * @brief Verify time clock sleep behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_time_clock_sleep(void **state)
{
	uint64_t first, last;
	(void)state;
	assert_int_equal(osi_time_get_ms(&first), 0);
	assert_true(test_now_ms() >= first);
	assert_int_equal(osi_sleep_ms(25), 0);
	assert_int_equal(osi_time_get_ms(&last), 0);
	assert_true(last >= first + 24);
	assert_int_equal(osi_sleep_ms(0), 0);
	assert_int_equal(osi_sleep_ms(-1), -EINVAL);
	assert_int_equal(osi_sleep_ms(-2), -EINVAL);
	assert_int_equal(osi_time_get_ms(NULL), -EINVAL);
}

/**
 * @brief Check OSI microsecond/millisecond consistency and monotonic elapsed time.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_time_microseconds(void **state)
{
	uint64_t before_ms, after_ms, timestamp, next;

	(void)state;
	assert_int_equal(osi_time_get_us(NULL), -EINVAL);
	assert_int_equal(osi_time_get_ms(&before_ms), 0);
	assert_int_equal(osi_time_get_us(&timestamp), 0);
	assert_int_equal(osi_time_get_ms(&after_ms), 0);
	assert_true(before_ms <= timestamp / 1000 && timestamp / 1000 <= after_ms);
	assert_int_equal(osi_sleep_ms(10), 0);
	assert_int_equal(osi_time_get_us(&next), 0);
	assert_true(next >= timestamp + 9000);
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
		cmocka_unit_test(test_time_units),
		cmocka_unit_test(test_time_clock_sleep),
		cmocka_unit_test(test_time_microseconds),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
