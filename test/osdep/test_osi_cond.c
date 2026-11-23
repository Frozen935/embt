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
 * @brief Condition predicates, notification and reacquired mutex contracts.
 */

#include "support/test_support.h"

struct cond_context {
	os_mutex_t mutex;
	os_cond_t cond;
	os_sem_t ready;
	bool go;
};
struct cond_worker {
	struct cond_context *context;
	int result;
};

/**
 * @brief Wait for a protected predicate and publish results only after joining.
 * @param[in,out] argument Worker context retained until join.
 */
static void cond_waiter(void *argument)
{
	struct cond_worker *worker = argument;
	struct cond_context *context = worker->context;

	test_require(osi_mutex_lock(&context->mutex, OS_TIMEOUT_FOREVER));
	test_require(osi_sem_give(&context->ready));
	while (!context->go) {
		worker->result = osi_cond_wait(&context->cond, &context->mutex, TEST_TIMEOUT_MS);
		if (worker->result) {
			break;
		}
	}
	test_require(osi_mutex_unlock(&context->mutex));
}

/**
 * @brief Notify one waiter or broadcast after all predicates are registered.
 * @param[in] count Number of workers, one or four.
 */
static void run_notification(unsigned int count)
{
	struct cond_context context = {0};
	struct cond_worker arguments[4];
	os_thread_t workers[4];
	int notified;

	assert_int_equal(osi_mutex_init(&context.mutex, OS_MUTEX_NORMAL), 0);
	assert_int_equal(osi_cond_init(&context.cond), 0);
	assert_int_equal(osi_sem_init(&context.ready, 0, count), 0);
	for (unsigned int i = 0; i < count; i++) {
		arguments[i] = (struct cond_worker){&context, 0};
		test_require(osi_thread_create(&workers[i], cond_waiter, &arguments[i],
					       "cond-test", 0, 0));
		test_wait(&context.ready);
	}
	/* Owning the predicate mutex ensures the ready workers have released it to wait. */
	test_require(osi_mutex_lock(&context.mutex, OS_TIMEOUT_FOREVER));
	context.go = true;
	notified = count == 1 ? osi_cond_signal(&context.cond) : osi_cond_broadcast(&context.cond);
	test_require(osi_mutex_unlock(&context.mutex));
	for (unsigned int i = 0; i < count; i++) {
		test_require(osi_thread_join(&workers[i]));
	}
	assert_int_equal(osi_cond_deinit(&context.cond), 0);
	assert_int_equal(osi_mutex_deinit(&context.mutex), 0);
	assert_int_equal(osi_sem_deinit(&context.ready), 0);
	assert_int_equal(notified, 0);
	for (unsigned int i = 0; i < count; i++) {
		assert_int_equal(arguments[i].result, 0);
	}
}

/**
 * @brief Signal a single registered predicate waiter.
 * @param[in,out] state Unused cmocka state.
 */
static void test_cond_signal(void **state)
{
	(void)state;
	run_notification(1);
}

/**
 * @brief Broadcast to all registered predicate waiters.
 * @param[in,out] state Unused cmocka state.
 */
static void test_cond_broadcast(void **state)
{
	(void)state;
	run_notification(4);
}

/**
 * @brief A finite condition wait times out while retaining caller mutex ownership.
 * @param[in,out] state Unused cmocka state.
 */
static void test_cond_timeout(void **state)
{
	os_mutex_t mutex;
	os_cond_t cond;
	uint64_t start, elapsed;
	int timed, held;

	(void)state;
	assert_int_equal(osi_mutex_init(&mutex, OS_MUTEX_NORMAL), 0);
	assert_int_equal(osi_cond_init(&cond), 0);
	assert_int_equal(osi_mutex_lock(&mutex, OS_TIMEOUT_FOREVER), 0);
	assert_int_equal(osi_cond_wait(&cond, &mutex, OS_TIMEOUT_NO_WAIT), -EAGAIN);
	assert_int_equal(osi_cond_wait(&cond, &mutex, -2), -EINVAL);
	start = test_now_ms();
	timed = osi_cond_wait(&cond, &mutex, 25);
	elapsed = test_now_ms() - start;
	held = osi_mutex_lock(&mutex, OS_TIMEOUT_NO_WAIT);
	assert_int_equal(osi_mutex_unlock(&mutex), 0);
	assert_int_equal(osi_cond_deinit(&cond), 0);
	assert_int_equal(osi_mutex_deinit(&mutex), 0);
	assert_int_equal(timed, -ETIMEDOUT);
	assert_int_equal(held, -EAGAIN);
	assert_true(elapsed >= 24);
	assert_int_equal(osi_cond_init(NULL), -EINVAL);
	assert_int_equal(osi_cond_wait(NULL, NULL, 1), -EINVAL);
	assert_int_equal(osi_cond_signal(NULL), -EINVAL);
	assert_int_equal(osi_cond_broadcast(NULL), -EINVAL);
	assert_int_equal(osi_cond_deinit(NULL), -EINVAL);
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
		cmocka_unit_test(test_cond_signal),
		cmocka_unit_test(test_cond_broadcast),
		cmocka_unit_test(test_cond_timeout),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
