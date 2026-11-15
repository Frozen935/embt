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
 * @brief Recursive and contended mutex contracts using OSI workers.
 */

#include "support/test_support.h"

struct mutex_context {
	os_mutex_t mutex;
	os_sem_t ready, release;
	int lock_result, unlock_result;
};

/**
 * @brief Own the tested mutex until the main thread releases the OSI gate.
 * @param[in,out] argument Borrowed context retained until join.
 */
static void mutex_holder(void *argument)
{
	struct mutex_context *context = argument;

	context->lock_result = osi_mutex_lock(&context->mutex, OS_TIMEOUT_FOREVER);
	test_require(context->lock_result);
	test_require(osi_sem_give(&context->ready));
	test_wait(&context->release);
	context->unlock_result = osi_mutex_unlock(&context->mutex);
}

/**
 * @brief Balance recursive acquisition and reject invalid mutex arguments.
 * @param[in,out] state Unused cmocka state.
 */
static void test_mutex_recursive(void **state)
{
	os_mutex_t mutex;

	(void)state;
	assert_int_equal(osi_mutex_init(&mutex, OS_MUTEX_RECURSIVE), 0);
	assert_int_equal(osi_mutex_lock(&mutex, OS_TIMEOUT_NO_WAIT), 0);
	assert_int_equal(osi_mutex_lock(&mutex, 10), 0);
	assert_int_equal(osi_mutex_unlock(&mutex), 0);
	assert_int_equal(osi_mutex_unlock(&mutex), 0);
	assert_int_equal(osi_mutex_deinit(&mutex), 0);
	assert_int_equal(osi_mutex_init(&mutex, (os_mutex_kind_t)99), -EINVAL);
	assert_int_equal(osi_mutex_init(NULL, OS_MUTEX_NORMAL), -EINVAL);
	assert_int_equal(osi_mutex_lock(NULL, 0), -EINVAL);
	assert_int_equal(osi_mutex_unlock(NULL), -EINVAL);
	assert_int_equal(osi_mutex_deinit(NULL), -EINVAL);
}

/**
 * @brief Observe no-wait, finite timeout and acquisition after another owner releases.
 * @param[in,out] state Unused cmocka state.
 */
static void test_mutex_contention(void **state)
{
	struct mutex_context context = {0};
	os_thread_t worker;
	uint64_t start, elapsed;
	int nowait, timed, invalid, acquired;

	(void)state;
	assert_int_equal(osi_mutex_init(&context.mutex, OS_MUTEX_NORMAL), 0);
	assert_int_equal(osi_sem_init(&context.ready, 0, 1), 0);
	assert_int_equal(osi_sem_init(&context.release, 0, 1), 0);
	assert_int_equal(osi_thread_create(&worker, mutex_holder, &context, "mutex-test", 0, 0), 0);
	test_wait(&context.ready);
	nowait = osi_mutex_lock(&context.mutex, OS_TIMEOUT_NO_WAIT);
	start = test_now_ms();
	timed = osi_mutex_lock(&context.mutex, 25);
	elapsed = test_now_ms() - start;
	invalid = osi_mutex_lock(&context.mutex, -2);
	test_require(osi_sem_give(&context.release));
	acquired = osi_mutex_lock(&context.mutex, OS_TIMEOUT_FOREVER);
	if (!acquired) {
		test_require(osi_mutex_unlock(&context.mutex));
	}
	test_require(osi_thread_join(&worker));
	assert_int_equal(osi_mutex_deinit(&context.mutex), 0);
	assert_int_equal(osi_sem_deinit(&context.ready), 0);
	assert_int_equal(osi_sem_deinit(&context.release), 0);
	assert_int_equal(nowait, -EAGAIN);
	assert_int_equal(timed, -ETIMEDOUT);
	assert_true(elapsed >= 24);
	assert_int_equal(invalid, -EINVAL);
	assert_int_equal(acquired, 0);
	assert_int_equal(context.lock_result, 0);
	assert_int_equal(context.unlock_result, 0);
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
		cmocka_unit_test(test_mutex_recursive),
		cmocka_unit_test(test_mutex_contention),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
