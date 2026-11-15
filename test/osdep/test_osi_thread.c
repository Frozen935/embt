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
 * @brief All nine OSI thread operations, immediate start and cancellation cleanup.
 */

#include "support/test_support.h"

#include <string.h>
#include <limits.h>
struct thread_context {
	os_thread_t thread;
	sem_t ready, release;
	pthread_mutex_t mutex;
	atomic_uint runs, cleanup;
	os_tid_t self;
	bool current;
};
/**
 * @brief Publish entry execution and wait for the controlling thread.
 * @param[in,out] argument Borrowed thread test context.
 */
static void thread_worker(void *argument)
{
	struct thread_context *context = argument;
	atomic_fetch_add(&context->runs, 1);
	sem_post(&context->ready);
	test_wait(&context->release);
	context->self = osi_thread_self();
	context->current = osi_thread_is_current(&context->thread);
}
/**
 * @brief Verify thread lifecycle behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_thread_lifecycle(void **state)
{
	struct thread_context context = {0};
	char name[16];
	int created, started, named, joined, bad_name;
	bool parent_current, different;
	(void)state;
	sem_init(&context.ready, 0, 0);
	sem_init(&context.release, 0, 0);
	created = osi_thread_create(&context.thread, thread_worker, &context, "osi-create", 0, 0);
	if (created != 0) {
		_exit(1);
	}
	/* The entry runs without requiring the compatibility start call. */
	test_wait(&context.ready);
	started = osi_thread_start(&context.thread);
	osi_thread_start(&context.thread);
	bad_name = osi_thread_name_set(&context.thread, "0123456789abcdef");
	named = osi_thread_name_set(&context.thread, "osi-named");
	pthread_getname_np(context.thread.native, name, sizeof(name));
	parent_current = osi_thread_is_current(&context.thread);
	different = !osi_thread_id_equal(osi_thread_self(), context.thread.native);
	sem_post(&context.release);
	joined = osi_thread_join(&context.thread);
	sem_destroy(&context.ready);
	sem_destroy(&context.release);
	assert_int_equal(created, 0);
	assert_int_equal(started, 0);
	assert_int_equal(bad_name, -ERANGE);
	assert_int_equal(named, 0);
	assert_string_equal(name, "osi-named");
	assert_false(parent_current);
	assert_true(different);
	assert_int_equal(joined, 0);
	assert_int_equal(atomic_load(&context.runs), 1);
	assert_true(context.current);
	/* A joined identity is expired; compare active identities separately below. */
	assert_true(osi_thread_id_equal(osi_thread_self(), pthread_self()));
	assert_int_equal(osi_thread_yield(), 0);
}
/**
 * @brief Release the native mutex owned across the cancellation point.
 * @param[in,out] argument Borrowed cancellation test context.
 */
static void cancellation_cleanup(void *argument)
{
	struct thread_context *context = argument;
	pthread_mutex_unlock(&context->mutex);
	atomic_fetch_add(&context->cleanup, 1);
}
/**
 * @brief Sleep at a cancellation point with a registered cleanup handler.
 * @param[in,out] argument Borrowed cancellation test context.
 */
static void cancellable_entry(void *argument)
{
	struct thread_context *context = argument;
	pthread_mutex_lock(&context->mutex);
	pthread_cleanup_push(cancellation_cleanup, context);
	sem_post(&context->ready);
	osi_sleep_ms(60000);
	atomic_fetch_add(&context->runs, 1);
	pthread_cleanup_pop(1);
}
/**
 * @brief Verify thread cancel join behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_thread_cancel_join(void **state)
{
	struct thread_context context = {0};
	int cancelled, joined;
	(void)state;
	sem_init(&context.ready, 0, 0);
	pthread_mutex_init(&context.mutex, NULL);
	assert_int_equal(
		osi_thread_create(&context.thread, cancellable_entry, &context, NULL, 0, 0), 0);
	test_wait(&context.ready);
	cancelled = osi_thread_cancel(&context.thread);
	joined = osi_thread_join(&context.thread);
	sem_destroy(&context.ready);
	int unlocked = pthread_mutex_trylock(&context.mutex);
	if (unlocked == 0) {
		pthread_mutex_unlock(&context.mutex);
		pthread_mutex_destroy(&context.mutex);
	}
	assert_int_equal(unlocked, 0);
	assert_int_equal(cancelled, 0);
	assert_int_equal(joined, 0);
	assert_int_equal(atomic_load(&context.cleanup), 1);
	assert_int_equal(atomic_load(&context.runs), 0);
}
static void simple_entry(void *argument)
{
	(void)argument;
}
/**
 * @brief Verify thread attributes behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_thread_attributes(void **state)
{
	os_thread_t thread;
	(void)state;
	assert_int_equal(osi_thread_create(&thread, simple_entry, NULL, NULL, 1, 0), -ENOTSUP);
	assert_int_equal(osi_thread_create(&thread, simple_entry, NULL, NULL, 0, 1), -EINVAL);
	assert_int_equal(osi_thread_create(&thread, simple_entry, NULL, "0123456789abcdef", 0, 0),
			 -ERANGE);
	assert_int_equal(osi_thread_create(&thread, simple_entry, NULL, NULL, 0, 1024 * 1024), 0);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_int_equal(osi_thread_create(NULL, simple_entry, NULL, NULL, 0, 0), -EINVAL);
	assert_int_equal(osi_thread_start(NULL), -EINVAL);
	assert_int_equal(osi_thread_cancel(NULL), -EINVAL);
	assert_int_equal(osi_thread_join(NULL), -EINVAL);
	assert_int_equal(osi_thread_name_set(NULL, "name"), -EINVAL);
}

struct join_context {
	os_thread_t *target;
	sem_t ready, completed;
	int result;
};
/**
 * @brief Join the target and publish actual completion to the controller.
 * @param[in,out] argument Borrowed join test context.
 * @return NULL after join completion.
 */
static void *join_worker(void *argument)
{
	struct join_context *context = argument;
	sem_post(&context->ready);
	context->result = osi_thread_join(context->target);
	sem_post(&context->completed);
	return NULL;
}
/**
 * @brief Verify thread join blocking behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_thread_join_blocking(void **state)
{
	struct thread_context context = {0};
	struct join_context join = {.target = &context.thread};
	pthread_t joiner;
	struct timespec deadline;
	int blocked;
	(void)state;
	sem_init(&context.ready, 0, 0);
	sem_init(&context.release, 0, 0);
	sem_init(&join.ready, 0, 0);
	sem_init(&join.completed, 0, 0);
	assert_int_equal(osi_thread_create(&context.thread, thread_worker, &context, NULL, 0, 0),
			 0);
	test_wait(&context.ready);
	if (pthread_create(&joiner, NULL, join_worker, &join) != 0) {
		_exit(1);
	}
	test_wait(&join.ready);
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_nsec += 25000000;
	if (deadline.tv_nsec >= 1000000000) {
		deadline.tv_sec++;
		deadline.tv_nsec -= 1000000000;
	}
	blocked = sem_clockwait(&join.completed, CLOCK_MONOTONIC, &deadline) == -1 ? errno : 0;
	sem_post(&context.release);
	test_wait(&join.completed);
	pthread_join(joiner, NULL);
	sem_destroy(&context.ready);
	sem_destroy(&context.release);
	sem_destroy(&join.ready);
	sem_destroy(&join.completed);
	assert_int_equal(blocked, ETIMEDOUT);
	assert_int_equal(join.result, 0);
	assert_int_equal(atomic_load(&context.runs), 1);
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
		cmocka_unit_test(test_thread_lifecycle),
		cmocka_unit_test(test_thread_cancel_join),
		cmocka_unit_test(test_thread_attributes),
		cmocka_unit_test(test_thread_join_blocking),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
