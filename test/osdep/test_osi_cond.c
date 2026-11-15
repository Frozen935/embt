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
 * @brief Predicate locking, notification and condition-variable timeout tests.
 */

#include "support/test_support.h"

struct cond_context {
	os_mutex_t mutex;
	os_cond_t cond;
	sem_t ready;
	bool go;
	os_timeout_t timeout;
	atomic_int error;
	atomic_uint completed;
};
/**
 * @brief Wait under the predicate mutex and publish completion.
 * @param[in,out] argument Borrowed condition-variable test context.
 * @return NULL after releasing the mutex.
 */
static void *cond_waiter(void *argument)
{
	struct cond_context *context = argument;
	int rc = osi_mutex_lock(&context->mutex, OS_TIMEOUT_FOREVER);
	if (rc != 0) {
		atomic_store(&context->error, rc);
		return NULL;
	}
	sem_post(&context->ready);
	while (!context->go) {
		rc = osi_cond_wait(&context->cond, &context->mutex, context->timeout);
		if (rc != 0) {
			atomic_store(&context->error, rc);
			break;
		}
	}
	atomic_fetch_add(&context->completed, 1);
	osi_mutex_unlock(&context->mutex);
	return NULL;
}
/**
 * @brief Coordinate registered condition waiters before notifying them.
 * @param[in] count Number of waiter threads, one or four.
 */
static void run_notification(unsigned int count)
{
	struct cond_context context = {0};
	pthread_t workers[4];
	context.timeout = count == 1 ? OS_TIMEOUT_FOREVER : 1000;
	osi_mutex_init(&context.mutex, OS_MUTEX_NORMAL);
	osi_cond_init(&context.cond);
	sem_init(&context.ready, 0, 0);
	for (unsigned int i = 0; i < count; i++) {
		if (pthread_create(&workers[i], NULL, cond_waiter, &context) != 0) {
			_exit(1);
		}
		test_wait(&context.ready);
	}
	/* Acquiring the predicate mutex confirms each waiter registered and released it. */
	osi_mutex_lock(&context.mutex, OS_TIMEOUT_FOREVER);
	context.go = true;
	int notified =
		count == 1 ? osi_cond_signal(&context.cond) : osi_cond_broadcast(&context.cond);
	osi_mutex_unlock(&context.mutex);
	for (unsigned int i = 0; i < count; i++) {
		pthread_join(workers[i], NULL);
	}
	osi_cond_deinit(&context.cond);
	osi_mutex_deinit(&context.mutex);
	sem_destroy(&context.ready);
	assert_int_equal(notified, 0);
	assert_int_equal(atomic_load(&context.error), 0);
	assert_int_equal(atomic_load(&context.completed), count);
}
/**
 * @brief Verify cond signal behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_cond_signal(void **state)
{
	(void)state;
	run_notification(1);
}
/**
 * @brief Verify cond broadcast behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_cond_broadcast(void **state)
{
	(void)state;
	run_notification(4);
}
/**
 * @brief Verify cond timeout behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_cond_timeout(void **state)
{
	os_mutex_t mutex;
	os_cond_t cond;
	uint64_t began;
	(void)state;
	assert_int_equal(osi_mutex_init(&mutex, OS_MUTEX_NORMAL), 0);
	assert_int_equal(osi_cond_init(&cond), 0);
	assert_int_equal(osi_mutex_lock(&mutex, OS_TIMEOUT_FOREVER), 0);
	assert_int_equal(osi_cond_wait(&cond, &mutex, 0), -EAGAIN);
	assert_int_equal(osi_cond_wait(&cond, &mutex, -2), -EINVAL);
	began = test_now_ms();
	assert_int_equal(osi_cond_wait(&cond, &mutex, 25), -ETIMEDOUT);
	assert_true(test_now_ms() - began >= 24);
	/* A native trylock reports busy only if the caller still holds the normal mutex. */
	assert_int_equal(pthread_mutex_trylock(&mutex), EBUSY);
	assert_int_equal(osi_mutex_unlock(&mutex), 0);
	assert_int_equal(osi_cond_deinit(&cond), 0);
	assert_int_equal(osi_mutex_deinit(&mutex), 0);
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
		cmocka_unit_test(test_cond_signal),
		cmocka_unit_test(test_cond_broadcast),
		cmocka_unit_test(test_cond_timeout),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
