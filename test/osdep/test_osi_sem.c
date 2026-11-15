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
 * @brief Bounded semaphore credits, waiting and concurrent give tests.
 */

#include "support/test_support.h"

struct sem_context {
	os_sem_t sem;
	pthread_barrier_t barrier;
	atomic_int error;
};
/**
 * @brief Give bounded credits concurrently without framework calls in the worker.
 * @param[in,out] argument Borrowed semaphore test context.
 * @return NULL after giving credits.
 */
static void *sem_giver(void *argument)
{
	struct sem_context *context = argument;
	pthread_barrier_wait(&context->barrier);
	for (int i = 0; i < 300; i++) {
		int rc = osi_sem_give(&context->sem);
		if (rc != 0) {
			atomic_store(&context->error, rc);
		}
	}
	return NULL;
}
/**
 * @brief Verify sem count reset behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_sem_count_reset(void **state)
{
	os_sem_t sem;
	uint32_t count;
	(void)state;
	assert_int_equal(osi_sem_init(&sem, 2, 2), 0);
	assert_int_equal(osi_sem_give(&sem), 0);
	assert_int_equal(osi_sem_count_get(&sem, &count), 0);
	assert_int_equal(count, 2);
	assert_int_equal(osi_sem_take(&sem, 0), 0);
	assert_int_equal(osi_sem_reset(&sem), 0);
	assert_int_equal(osi_sem_take(&sem, 0), -EAGAIN);
	assert_int_equal(osi_sem_take(&sem, -2), -EINVAL);
	assert_int_equal(osi_sem_count_get(&sem, &count), 0);
	assert_int_equal(count, 0);
	assert_int_equal(osi_sem_deinit(&sem), 0);
	assert_int_equal(osi_sem_init(&sem, 0, 0), -EINVAL);
	assert_int_equal(osi_sem_init(&sem, 2, 1), -EINVAL);
	assert_int_equal(osi_sem_init(&sem, 0, UINT32_MAX), -EINVAL);
}
/**
 * @brief Verify sem concurrent limit behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_sem_concurrent_limit(void **state)
{
	struct sem_context context = {0};
	pthread_t workers[8];
	uint32_t count;
	(void)state;
	assert_int_equal(osi_sem_init(&context.sem, 0, 1), 0);
	pthread_barrier_init(&context.barrier, NULL, 8);
	for (size_t i = 0; i < 8; i++) {
		if (pthread_create(&workers[i], NULL, sem_giver, &context) != 0) {
			_exit(1);
		}
	}
	for (size_t i = 0; i < 8; i++) {
		pthread_join(workers[i], NULL);
	}
	osi_sem_count_get(&context.sem, &count);
	osi_sem_deinit(&context.sem);
	pthread_barrier_destroy(&context.barrier);
	assert_int_equal(atomic_load(&context.error), 0);
	assert_int_equal(count, 1);
}
/**
 * @brief Produce semaphore credits for the controlling consumer.
 * @param[in,out] argument Borrowed semaphore test context.
 * @return NULL after producing credits.
 */
static void *sem_credit(void *argument)
{
	struct sem_context *context = argument;
	for (int i = 0; i < 200; i++) {
		int rc = osi_sem_give(&context->sem);
		if (rc != 0) {
			atomic_store(&context->error, rc);
		}
	}
	return NULL;
}
/**
 * @brief Verify sem wait credits behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_sem_wait_credits(void **state)
{
	struct sem_context context = {0};
	pthread_t producer;
	uint64_t began;
	int timed, rc = 0;
	(void)state;
	assert_int_equal(osi_sem_init(&context.sem, 0, 200), 0);
	began = test_now_ms();
	timed = osi_sem_take(&context.sem, 25);
	assert_int_equal(timed, -ETIMEDOUT);
	assert_true(test_now_ms() - began >= 24);
	assert_int_equal(pthread_create(&producer, NULL, sem_credit, &context), 0);
	for (int i = 0; i < 200; i++) {
		int current = osi_sem_take(&context.sem, OS_TIMEOUT_FOREVER);
		if (current != 0) {
			rc = current;
		}
	}
	pthread_join(producer, NULL);
	osi_sem_deinit(&context.sem);
	assert_int_equal(rc, 0);
	assert_int_equal(atomic_load(&context.error), 0);
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
		cmocka_unit_test(test_sem_count_reset),
		cmocka_unit_test(test_sem_concurrent_limit),
		cmocka_unit_test(test_sem_wait_credits),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
