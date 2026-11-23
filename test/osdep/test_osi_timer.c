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
 * @brief One-shot timer contracts with OSI notifications and stable callback storage.
 */

#include "support/test_support.h"

struct timer_context {
	os_timer_t timer;
	os_sem_t notified;
};

/**
 * @brief Publish expiration without exposing backend fields or framework state.
 * @param[in] source Expiring OSI timer.
 * @param[in,out] argument Application-lifetime context, never reused by another case.
 */
static void timer_callback(os_timer_t *source, void *argument)
{
	struct timer_context *context = argument;

	test_require(source == &context->timer ? 0 : -EINVAL);
	test_require(osi_sem_give(&context->notified));
}

/**
 * @brief Create a timer with storage retained for already-dispatched callbacks.
 * @param[in,out] context Unique static context owned by one case in this application run.
 * @note Notification semaphore stays alive through application termination, including after delete.
 */
static void context_init(struct timer_context *context)
{
	assert_int_equal(osi_sem_init(&context->notified, 0, 4), 0);
	assert_int_equal(osi_timer_create(&context->timer, timer_callback, context), 0);
}

/**
 * @brief A positive one-shot delay produces its OSI notification and rejects invalid requests.
 * @param[in,out] state Unused cmocka state.
 */
static void test_timer_expire(void **state)
{
	static struct timer_context context;
	os_timer_t invalid;
	uint64_t remaining, start, elapsed;
	int notified;

	(void)state;
	context_init(&context);
	assert_int_equal(osi_timer_remaining_ms(&context.timer, &remaining), 0);
	assert_int_equal(remaining, 0);
	assert_int_equal(osi_timer_start(&context.timer, 0), -EINVAL);
	assert_int_equal(osi_timer_start(&context.timer, -1), -EINVAL);
	start = test_now_ms();
	assert_int_equal(osi_timer_start(&context.timer, 30), 0);
	notified = osi_sem_take(&context.notified, TEST_TIMEOUT_MS);
	elapsed = test_now_ms() - start;
	assert_int_equal(osi_timer_stop(&context.timer), 0);
	assert_int_equal(osi_timer_delete(&context.timer), 0);
	assert_int_equal(notified, 0);
	assert_true(elapsed >= 29);
	assert_int_equal(osi_timer_create(NULL, timer_callback, NULL), -EINVAL);
	assert_int_equal(osi_timer_create(&invalid, NULL, NULL), -EINVAL);
	assert_int_equal(osi_timer_start(NULL, 1), -EINVAL);
	assert_int_equal(osi_timer_stop(NULL), -EINVAL);
	assert_int_equal(osi_timer_delete(NULL), -EINVAL);
	assert_int_equal(osi_timer_remaining_ms(NULL, &remaining), -EINVAL);
}

/**
 * @brief Rearm a distant expiration, stop it and observe an inactive schedule.
 * @param[in,out] state Unused cmocka state.
 */
static void test_timer_rearm_stop(void **state)
{
	static struct timer_context context;
	uint64_t remaining;
	int unexpected;

	(void)state;
	context_init(&context);
	assert_int_equal(osi_timer_start(&context.timer, 60000), 0);
	assert_int_equal(osi_timer_remaining_ms(&context.timer, &remaining), 0);
	assert_true(remaining > 0 && remaining <= 60000);
	assert_int_equal(osi_timer_start(&context.timer, 30000), 0);
	assert_int_equal(osi_timer_remaining_ms(&context.timer, &remaining), 0);
	assert_true(remaining > 0 && remaining <= 30000);
	assert_int_equal(osi_timer_stop(&context.timer), 0);
	assert_int_equal(osi_timer_remaining_ms(&context.timer, &remaining), 0);
	assert_int_equal(remaining, 0);
	unexpected = osi_sem_take(&context.notified, 25);
	assert_int_equal(osi_timer_delete(&context.timer), 0);
	assert_int_equal(unexpected, -ETIMEDOUT);
}

/**
 * @brief A short replacement schedule delivers one notification instead of the distant one.
 * @param[in,out] state Unused cmocka state.
 */
static void test_timer_rearm_expire(void **state)
{
	static struct timer_context context;
	int notified, extra;

	(void)state;
	context_init(&context);
	assert_int_equal(osi_timer_start(&context.timer, 60000), 0);
	assert_int_equal(osi_timer_start(&context.timer, 20), 0);
	notified = osi_sem_take(&context.notified, TEST_TIMEOUT_MS);
	extra = osi_sem_take(&context.notified, 25);
	assert_int_equal(osi_timer_stop(&context.timer), 0);
	assert_int_equal(osi_timer_delete(&context.timer), 0);
	assert_int_equal(notified, 0);
	assert_int_equal(extra, -ETIMEDOUT);
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
		cmocka_unit_test(test_timer_expire),
		cmocka_unit_test(test_timer_rearm_stop),
		cmocka_unit_test(test_timer_rearm_expire),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
