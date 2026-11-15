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
 * @brief Process-isolated timer scheduling and retained notification lifetime tests.
 */

#include "support/test_support.h"

static os_timer_t timer;
static atomic_uint expired;
/**
 * @brief Publish native expiration without touching framework state.
 * @param[in] source Native notification source, unused.
 * @param[in,out] argument Process-lifetime atomic notification count.
 */
static void timer_callback(os_timer_t *source, void *argument)
{
	atomic_uint *count = argument;
	(void)source;
	atomic_fetch_add(count, 1);
}
/**
 * @brief Verify timer expire behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_timer_expire(void **state)
{
	uint64_t remaining;
	bool notified;
	(void)state;
	assert_int_equal(osi_timer_create(&timer, timer_callback, &expired), 0);
	assert_int_equal(osi_timer_remaining_ms(&timer, &remaining), 0);
	assert_int_equal(remaining, 0);
	assert_int_equal(osi_timer_start(&timer, 0), -EINVAL);
	assert_int_equal(osi_timer_start(&timer, -1), -EINVAL);
	assert_int_equal(osi_timer_start(&timer, 30), 0);
	notified = test_wait_count(&expired, 1);
	assert_int_equal(osi_timer_stop(&timer), 0);
	assert_int_equal(osi_timer_delete(&timer), 0);
	assert_true(notified);
	assert_non_null(timer.callback);
	assert_ptr_equal(timer.argument, &expired);
}
/**
 * @brief Verify timer rearm stop behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_timer_rearm_stop(void **state)
{
	uint64_t remaining;
	(void)state;
	assert_int_equal(osi_timer_create(&timer, timer_callback, &expired), 0);
	assert_int_equal(osi_timer_start(&timer, 60000), 0);
	assert_int_equal(osi_timer_remaining_ms(&timer, &remaining), 0);
	assert_true(remaining > 0 && remaining <= 60000);
	assert_int_equal(osi_timer_start(&timer, 30000), 0);
	assert_int_equal(osi_timer_remaining_ms(&timer, &remaining), 0);
	assert_true(remaining > 0 && remaining <= 30000);
	assert_int_equal(osi_timer_stop(&timer), 0);
	assert_int_equal(osi_timer_remaining_ms(&timer, &remaining), 0);
	assert_int_equal(remaining, 0);
	assert_int_equal(osi_timer_delete(&timer), 0);
	assert_int_equal(atomic_load(&expired), 0);
	assert_non_null(timer.callback);
}

/**
 * @brief Verify timer rearm expire behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_timer_rearm_expire(void **state)
{
	bool notified;
	(void)state;
	assert_int_equal(osi_timer_create(&timer, timer_callback, &expired), 0);
	assert_int_equal(osi_timer_start(&timer, 60000), 0);
	assert_int_equal(osi_timer_start(&timer, 20), 0);
	notified = test_wait_count(&expired, 1);
	assert_int_equal(osi_timer_stop(&timer), 0);
	assert_int_equal(osi_timer_delete(&timer), 0);
	assert_true(notified);
	assert_int_equal(atomic_load(&expired), 1);
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
		cmocka_unit_test(test_timer_expire),
		cmocka_unit_test(test_timer_rearm_stop),
		cmocka_unit_test(test_timer_rearm_expire),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
