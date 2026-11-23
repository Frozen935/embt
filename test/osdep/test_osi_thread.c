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
 * @brief OSI thread lifecycle, identity, cancellation submission and join.
 */

#include "support/test_support.h"

struct thread_context {
	os_thread_t thread;
	os_sem_t begin, ready, release;
	os_thread_t self;
	bool current;
	unsigned int runs;
	int self_join;
};

/**
 * @brief Publish identity while live and wait until the main thread finishes observing it.
 * @param[in,out] argument Context retained until join.
 */
static void thread_worker(void *argument)
{
	struct thread_context *context = argument;

	test_wait(&context->begin);
	context->self = osi_thread_self();
	context->current = osi_thread_is_current(&context->thread);
	context->self_join = osi_thread_join(&context->thread);
	context->runs++;
	test_require(osi_sem_give(&context->ready));
	test_wait(&context->release);
}

/**
 * @brief Initialize notification gates before a worker can run.
 * @param[in,out] context Zero-initialized test context.
 */
static void context_init(struct thread_context *context)
{
	assert_int_equal(osi_sem_init(&context->begin, 0, 1), 0);
	assert_int_equal(osi_sem_init(&context->ready, 0, 1), 0);
	assert_int_equal(osi_sem_init(&context->release, 0, 1), 0);
}

/**
 * @brief Release gates after the worker has been joined.
 * @param[in,out] context Joined test context.
 */
static void context_deinit(struct thread_context *context)
{
	assert_int_equal(osi_sem_deinit(&context->begin), 0);
	assert_int_equal(osi_sem_deinit(&context->ready), 0);
	assert_int_equal(osi_sem_deinit(&context->release), 0);
}

/**
 * @brief Create, start, name, compare active identities and join an OSI worker.
 * @param[in,out] state Unused cmocka state.
 */
static void test_thread_lifecycle(void **state)
{
	struct thread_context context = {0};
	bool same, parent_current;
	int named, invalid_name;

	(void)state;
	context_init(&context);
	assert_int_equal(osi_thread_create(&context.thread, thread_worker, &context,
					 "osi-create", 0, 0), 0);
	test_require(osi_thread_start(&context.thread));
	test_require(osi_sem_give(&context.begin));
	test_wait(&context.ready);
	same = osi_thread_equal(osi_thread_self(), context.self);
	parent_current = osi_thread_is_current(&context.thread);
	named = osi_thread_name_set(&context.thread, "osi-named");
	invalid_name = osi_thread_name_set(&context.thread, NULL);
	test_require(osi_sem_give(&context.release));
	test_require(osi_thread_join(&context.thread));
	context_deinit(&context);
	assert_int_equal(named, 0);
	assert_int_equal(invalid_name, -EINVAL);
	assert_false(same);
	assert_false(parent_current);
	assert_true(context.current);
	assert_int_equal(context.self_join, -EDEADLK);
	assert_int_equal(context.runs, 1);
	assert_true(osi_thread_equal(osi_thread_self(), osi_thread_self()));
	assert_int_equal(osi_thread_yield(), 0);
}

/**
 * @brief Compare created, borrowed and copied handles while both workers remain live.
 * @param[in,out] state Unused cmocka state.
 */
static void test_thread_handle_identity(void **state)
{
	struct thread_context first = {0}, second = {0};
	os_thread_t first_copy, self;
	bool created_matches, copied_matches, distinct, symmetric, current;

	(void)state;
	context_init(&first);
	context_init(&second);
	test_require(osi_thread_create(&first.thread, thread_worker, &first, "handle-a", 0, 0));
	test_require(osi_thread_create(&second.thread, thread_worker, &second, "handle-b", 0, 0));
	test_require(osi_sem_give(&first.begin));
	test_require(osi_sem_give(&second.begin));
	test_wait(&first.ready);
	test_wait(&second.ready);
	first_copy = first.thread;
	self = osi_thread_self();
	created_matches = osi_thread_equal(first.thread, first.self) &&
		osi_thread_equal(second.thread, second.self);
	copied_matches = osi_thread_equal(first_copy, first.self);
	distinct = !osi_thread_equal(first.thread, second.thread) &&
		!osi_thread_equal(self, first_copy);
	symmetric = osi_thread_equal(first.self, first_copy);
	current = osi_thread_is_current(&self) && !osi_thread_is_current(&first_copy);
	test_require(osi_sem_give(&first.release));
	test_require(osi_sem_give(&second.release));
	/* Only the original owning handles are joined; borrowed copies add no ownership. */
	test_require(osi_thread_join(&first.thread));
	test_require(osi_thread_join(&second.thread));
	context_deinit(&first);
	context_deinit(&second);
	assert_true(created_matches);
	assert_true(copied_matches);
	assert_true(distinct);
	assert_true(symmetric);
	assert_true(current);
}

/**
 * @brief Reject invalid thread arguments without accessing uninitialized native handles.
 * @param[in,out] state Unused cmocka state.
 */
static void test_thread_invalid(void **state)
{
	os_thread_t thread;

	(void)state;
	assert_int_equal(osi_thread_create(NULL, thread_worker, NULL, NULL, 0, 0), -EINVAL);
	assert_int_equal(osi_thread_create(&thread, NULL, NULL, NULL, 0, 0), -EINVAL);
	assert_int_equal(osi_thread_start(NULL), -EINVAL);
	assert_int_equal(osi_thread_cancel(NULL), -EINVAL);
	assert_int_equal(osi_thread_join(NULL), -EINVAL);
	assert_false(osi_thread_is_current(NULL));
	assert_int_equal(osi_thread_name_set(NULL, "name"), -EINVAL);
}

struct join_context {
	os_thread_t *target;
	os_sem_t started, completed;
	int result;
};

/**
 * @brief Perform the only join of the target and notify its completion.
 * @param[in,out] argument Join context retained until the joiner itself terminates.
 */
static void join_worker(void *argument)
{
	struct join_context *context = argument;

	test_require(osi_sem_give(&context->started));
	context->result = osi_thread_join(context->target);
	test_require(osi_sem_give(&context->completed));
}

/**
 * @brief Join remains blocked while a live target is held at an OSI gate.
 * @param[in,out] state Unused cmocka state.
 */
static void test_thread_join_waits(void **state)
{
	struct thread_context context = {0};
	struct join_context join = {.target = &context.thread};
	os_thread_t joiner;
	int blocked;

	(void)state;
	context_init(&context);
	assert_int_equal(osi_sem_init(&join.started, 0, 1), 0);
	assert_int_equal(osi_sem_init(&join.completed, 0, 1), 0);
	assert_int_equal(osi_thread_create(&context.thread, thread_worker, &context,
					 "target", 0, 0), 0);
	test_require(osi_sem_give(&context.begin));
	test_wait(&context.ready);
	test_require(osi_thread_create(&joiner, join_worker, &join, "joiner", 0, 0));
	test_wait(&join.started);
	blocked = osi_sem_take(&join.completed, 25);
	test_require(osi_sem_give(&context.release));
	test_wait(&join.completed);
	test_require(osi_thread_join(&joiner));
	context_deinit(&context);
	assert_int_equal(osi_sem_deinit(&join.started), 0);
	assert_int_equal(osi_sem_deinit(&join.completed), 0);
	assert_int_equal(blocked, -ETIMEDOUT);
	assert_int_equal(join.result, 0);
	assert_int_equal(context.runs, 1);
}

/**
 * @brief Submit cancellation and join without assuming application-resource cleanup.
 * @param[in,out] state Unused cmocka state.
 */
static void test_thread_cancel_join(void **state)
{
	struct thread_context context = {0};
	int canceled;

	(void)state;
	context_init(&context);
	assert_int_equal(osi_thread_create(&context.thread, thread_worker, &context,
					 "cancel", 0, 0), 0);
	test_require(osi_sem_give(&context.begin));
	test_wait(&context.ready);
	canceled = osi_thread_cancel(&context.thread);
	/* A successful request is not immediate termination; allow cooperative completion too. */
	test_require(osi_sem_give(&context.release));
	test_require(osi_thread_join(&context.thread));
	context_deinit(&context);
	assert_int_equal(canceled, 0);
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
		cmocka_unit_test(test_thread_lifecycle),
		cmocka_unit_test(test_thread_handle_identity),
		cmocka_unit_test(test_thread_invalid),
		cmocka_unit_test(test_thread_join_waits),
		cmocka_unit_test(test_thread_cancel_join),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
