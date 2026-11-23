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
 * @brief Bounded semaphore credits, concurrency and waiting through OSI.
 */

#include "support/test_support.h"

struct sem_context {
	os_sem_t sem, gate, ready;
	int results[8];
};
struct sem_worker {
	struct sem_context *context;
	size_t index;
	unsigned int count;
	bool gated;
};

/**
 * @brief Produce credits and keep each worker result separate until join.
 * @param[in,out] argument Worker context retained until join.
 */
static void sem_giver(void *argument)
{
	struct sem_worker *worker = argument;

	if (worker->gated) {
		test_require(osi_sem_give(&worker->context->ready));
		test_wait(&worker->context->gate);
	}
	for (unsigned int i = 0; i < worker->count; i++) {
		int result = osi_sem_give(&worker->context->sem);

		if (result) {
			worker->context->results[worker->index] = result;
		}
	}
}

/**
 * @brief Saturate the configured bound, consume and reset only quiescent credits.
 * @param[in,out] state Unused cmocka state.
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
	assert_int_equal(osi_sem_take(&sem, OS_TIMEOUT_NO_WAIT), 0);
	assert_int_equal(osi_sem_reset(&sem), 0);
	assert_int_equal(osi_sem_take(&sem, OS_TIMEOUT_NO_WAIT), -EAGAIN);
	assert_int_equal(osi_sem_take(&sem, -2), -EINVAL);
	assert_int_equal(osi_sem_count_get(&sem, &count), 0);
	assert_int_equal(osi_sem_count_get(&sem, NULL), -EINVAL);
	assert_int_equal(count, 0);
	assert_int_equal(osi_sem_deinit(&sem), 0);
	assert_int_equal(osi_sem_init(&sem, 0, 0), -EINVAL);
	assert_int_equal(osi_sem_init(&sem, 2, 1), -EINVAL);
	assert_int_equal(osi_sem_init(NULL, 0, 1), -EINVAL);
	assert_int_equal(osi_sem_take(NULL, 0), -EINVAL);
	assert_int_equal(osi_sem_give(NULL), -EINVAL);
	assert_int_equal(osi_sem_reset(NULL), -EINVAL);
	assert_int_equal(osi_sem_deinit(NULL), -EINVAL);
	assert_int_equal(osi_sem_count_get(NULL, &count), -EINVAL);
}

/**
 * @brief Concurrent producers never exceed the declared semaphore limit.
 * @param[in,out] state Unused cmocka state.
 */
static void test_sem_concurrent_limit(void **state)
{
	struct sem_context context = {0};
	struct sem_worker arguments[8];
	os_thread_t workers[8];
	uint32_t count;

	(void)state;
	assert_int_equal(osi_sem_init(&context.sem, 0, 1), 0);
	assert_int_equal(osi_sem_init(&context.gate, 0, 8), 0);
	assert_int_equal(osi_sem_init(&context.ready, 0, 8), 0);
	for (size_t i = 0; i < TEST_ARRAY_SIZE(workers); i++) {
		arguments[i] = (struct sem_worker){&context, i, 300, true};
		test_require(osi_thread_create(&workers[i], sem_giver, &arguments[i],
					       "sem-test", 0, 0));
	}
	for (size_t i = 0; i < TEST_ARRAY_SIZE(workers); i++) {
		test_wait(&context.ready);
	}
	for (size_t i = 0; i < TEST_ARRAY_SIZE(workers); i++) {
		test_require(osi_sem_give(&context.gate));
	}
	for (size_t i = 0; i < TEST_ARRAY_SIZE(workers); i++) {
		test_require(osi_thread_join(&workers[i]));
	}
	assert_int_equal(osi_sem_count_get(&context.sem, &count), 0);
	assert_int_equal(osi_sem_deinit(&context.sem), 0);
	assert_int_equal(osi_sem_deinit(&context.gate), 0);
	assert_int_equal(osi_sem_deinit(&context.ready), 0);
	assert_int_equal(count, 1);
	for (size_t i = 0; i < TEST_ARRAY_SIZE(workers); i++) {
		assert_int_equal(context.results[i], 0);
	}
}

/**
 * @brief An empty semaphore times out and subsequently transfers all producer credits.
 * @param[in,out] state Unused cmocka state.
 */
static void test_sem_wait_credits(void **state)
{
	struct sem_context context = {0};
	struct sem_worker argument = {&context, 0, 200, false};
	os_thread_t producer;
	uint64_t start, elapsed;
	int timed;

	(void)state;
	assert_int_equal(osi_sem_init(&context.sem, 0, 200), 0);
	start = test_now_ms();
	timed = osi_sem_take(&context.sem, 25);
	elapsed = test_now_ms() - start;
	assert_int_equal(osi_thread_create(&producer, sem_giver, &argument, "credits", 0, 0), 0);
	for (unsigned int i = 0; i < argument.count; i++) {
		test_require(osi_sem_take(&context.sem, OS_TIMEOUT_FOREVER));
	}
	test_require(osi_thread_join(&producer));
	assert_int_equal(osi_sem_deinit(&context.sem), 0);
	assert_int_equal(timed, -ETIMEDOUT);
	assert_true(elapsed >= 24);
	assert_int_equal(context.results[0], 0);
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
		cmocka_unit_test(test_sem_count_reset),
		cmocka_unit_test(test_sem_concurrent_limit),
		cmocka_unit_test(test_sem_wait_credits),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
