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
 * @brief Recursive, contended and finite native mutex behavior tests.
 */

#include "support/test_support.h"

struct mutex_context {
	os_mutex_t mutex;
	sem_t ready, release;
	int lock_rc, unlock_rc;
};
/**
 * @brief Hold the OSI mutex until the controller explicitly releases the worker.
 * @param[in,out] argument Borrowed mutex test context.
 * @return NULL after unlocking.
 */
static void *mutex_holder(void *argument)
{
	struct mutex_context *context = argument;
	context->lock_rc = osi_mutex_lock(&context->mutex, OS_TIMEOUT_FOREVER);
	sem_post(&context->ready);
	test_wait(&context->release);
	context->unlock_rc = osi_mutex_unlock(&context->mutex);
	return NULL;
}
/**
 * @brief Verify mutex recursive behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_mutex_recursive(void **state)
{
	os_mutex_t mutex;
	(void)state;
	assert_int_equal(osi_mutex_init(&mutex, OS_MUTEX_RECURSIVE), 0);
	assert_int_equal(osi_mutex_lock(&mutex, 0), 0);
	assert_int_equal(osi_mutex_lock(&mutex, 10), 0);
	assert_int_equal(osi_mutex_unlock(&mutex), 0);
	assert_int_equal(osi_mutex_unlock(&mutex), 0);
	assert_int_equal(osi_mutex_deinit(&mutex), 0);
	assert_int_equal(osi_mutex_init(&mutex, (os_mutex_kind_t)99), -EINVAL);
	assert_int_equal(osi_mutex_lock(NULL, 0), -EINVAL);
}
/**
 * @brief Verify mutex contention behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_mutex_contention(void **state)
{
	struct mutex_context context = {0};
	pthread_t worker;
	uint64_t began, elapsed;
	int nowait, timed, invalid, acquired;
	(void)state;
	assert_int_equal(osi_mutex_init(&context.mutex, OS_MUTEX_NORMAL), 0);
	sem_init(&context.ready, 0, 0);
	sem_init(&context.release, 0, 0);
	assert_int_equal(pthread_create(&worker, NULL, mutex_holder, &context), 0);
	test_wait(&context.ready);
	nowait = osi_mutex_lock(&context.mutex, 0);
	began = test_now_ms();
	timed = osi_mutex_lock(&context.mutex, 25);
	elapsed = test_now_ms() - began;
	invalid = osi_mutex_lock(&context.mutex, -2);
	sem_post(&context.release);
	acquired = osi_mutex_lock(&context.mutex, OS_TIMEOUT_FOREVER);
	if (acquired == 0) {
		osi_mutex_unlock(&context.mutex);
	}
	pthread_join(worker, NULL);
	osi_mutex_deinit(&context.mutex);
	sem_destroy(&context.ready);
	sem_destroy(&context.release);
	assert_int_equal(nowait, -EAGAIN);
	assert_int_equal(timed, -ETIMEDOUT);
	assert_true(elapsed >= 24);
	assert_int_equal(invalid, -EINVAL);
	assert_int_equal(acquired, 0);
	assert_int_equal(context.lock_rc, 0);
	assert_int_equal(context.unlock_rc, 0);
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
		cmocka_unit_test(test_mutex_recursive),
		cmocka_unit_test(test_mutex_contention),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
