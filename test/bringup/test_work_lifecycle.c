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

/** @file @brief Explicit work-queue lifecycle, failure and cross-queue behavior. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <embt/base/work.h>
#include <embt/base/atomic.h>
#include <embt/osi/osi.h>

int __real_osi_thread_create(os_thread_t *, void (*)(void *), void *, const char *, int, size_t);
int __real_osi_sem_init(os_sem_t *, uint32_t, uint32_t);
static bool fail_thread;
static bool fail_sem;

int __wrap_osi_thread_create(os_thread_t *t, void (*entry)(void *), void *arg,
			     const char *name, int priority, size_t stack_size)
{
	return fail_thread ? -EAGAIN : __real_osi_thread_create(t, entry, arg, name, priority,
							    stack_size);
}

int __wrap_osi_sem_init(os_sem_t *sem, uint32_t count, uint32_t limit)
{
	return fail_sem ? -ENOMEM : __real_osi_sem_init(sem, count, limit);
}

/* Work queues must not allocate a separate runtime through OSI. */
void *__wrap_osi_calloc(size_t count, size_t size)
{
	(void)count;
	(void)size;
	return NULL;
}

struct work_context {
	struct bt_work work;
	struct bt_work_q *queue;
	os_sem_t entered;
	os_sem_t release;
	bt_atomic_t calls;
	bt_atomic_t wrong_thread;
	bool block;
	bool stop_self;
	int stop_result;
};

static void handler(struct bt_work *work)
{
	struct work_context *ctx = CONTAINER_OF(work, struct work_context, work);

	if (!osi_thread_is_current(bt_work_queue_thread_get(ctx->queue))) {
		bt_atomic_set(&ctx->wrong_thread, 1);
	}
	bt_atomic_inc(&ctx->calls);
	if (ctx->stop_self) {
		ctx->stop_result = bt_work_queue_stop(ctx->queue);
	}
	osi_sem_give(&ctx->entered);
	if (ctx->block) {
		osi_sem_take(&ctx->release, OS_TIMEOUT_FOREVER);
	}
}

static void context_init(struct work_context *ctx, struct bt_work_q *queue)
{
	ctx->queue = queue;
	assert_int_equal(osi_sem_init(&ctx->entered, 0, 4), 0);
	assert_int_equal(osi_sem_init(&ctx->release, 0, 4), 0);
	bt_work_init(&ctx->work, handler);
}

static void context_deinit(struct work_context *ctx)
{
	assert_int_equal(osi_sem_deinit(&ctx->entered), 0);
	assert_int_equal(osi_sem_deinit(&ctx->release), 0);
}

static void test_lifecycle(void **state)
{
	struct bt_work_q queue = {0};
	struct work_context ctx = {0};
	struct bt_work_sync sync;

	(void)state;
	assert_int_equal(bt_work_queue_start(&queue, 0, 0, NULL), -EINVAL);
	fail_sem = true;
	assert_int_equal(bt_work_queue_init(&queue), -ENOMEM);
	assert_false(queue.initialized);
	fail_sem = false;
	fail_thread = true;
	assert_int_equal(bt_work_queue_init(&queue), -EAGAIN);
	assert_false(queue.initialized);
	fail_thread = false;
	assert_int_equal(bt_work_queue_init(&queue), 0);
	assert_int_equal(bt_work_queue_init(&queue), 0);
	fail_thread = true;
	assert_int_equal(bt_work_queue_start(&queue, 0, 0, NULL), -EAGAIN);
	assert_false(queue.started);
	assert_true(queue.initialized);
	fail_thread = false;
	context_init(&ctx, &queue);
	for (int i = 0; i < 3; i++) {
		assert_int_equal(bt_work_queue_start(&queue, 0, 0,
			&(const struct bt_work_queue_config){.name = "work-test"}), 0);
		assert_int_equal(bt_work_queue_start(&queue, 0, 0, NULL), -EALREADY);
		assert_int_equal(bt_work_queue_init(&queue), -EALREADY);
		assert_int_equal(bt_work_queue_deinit(&queue), -EBUSY);
		assert_int_equal(bt_work_submit_to_queue(&queue, &ctx.work), 1);
		assert_int_equal(osi_sem_take(&ctx.entered, 1000), 0);
		bt_work_flush(&ctx.work, &sync);
		assert_int_equal(bt_work_queue_drain(&queue, true), 0);
		assert_int_equal(bt_work_queue_stop(&queue), 0);
		assert_true(queue.initialized);
		assert_false(queue.started);
		assert_int_equal(bt_work_submit_to_queue(&queue, &ctx.work), -ENODEV);
	}
	assert_int_equal(bt_atomic_get(&ctx.calls), 3);
	assert_int_equal(bt_atomic_get(&ctx.wrong_thread), 0);
	assert_int_equal(bt_work_queue_deinit(&queue), 0);
	assert_int_equal(bt_work_queue_deinit(&queue), 0);
	assert_int_equal(bt_work_queue_start(&queue, 0, 0, NULL), -EINVAL);
	context_deinit(&ctx);
}

static void test_cross_queue(void **state)
{
	struct bt_work_q first = {0}, second = {0};
	struct work_context ctx = {.block = true};
	struct bt_work_sync sync;

	(void)state;
	assert_int_equal(bt_work_queue_init(&first), 0);
	assert_int_equal(bt_work_queue_init(&second), 0);
	assert_int_equal(bt_work_queue_start(&first, 0, 0, NULL), 0);
	assert_int_equal(bt_work_queue_start(&second, 0, 0, NULL), 0);
	context_init(&ctx, &first);
	assert_int_equal(bt_work_submit_to_queue(&first, &ctx.work), 1);
	assert_int_equal(osi_sem_take(&ctx.entered, 1000), 0);
	assert_int_equal(bt_work_submit_to_queue(&second, &ctx.work), 2);
	assert_ptr_equal(ctx.work.queue, &first);
	assert_int_equal(bt_work_submit_to_queue(&second, &ctx.work), 0);
	osi_sem_give(&ctx.release);
	assert_int_equal(osi_sem_take(&ctx.entered, 1000), 0);
	osi_sem_give(&ctx.release);
	bt_work_flush(&ctx.work, &sync);
	assert_int_equal(bt_atomic_get(&ctx.calls), 2);
	assert_int_equal(bt_atomic_get(&ctx.wrong_thread), 0);
	assert_int_equal(bt_work_queue_drain(&first, true), 0);
	assert_int_equal(bt_work_queue_stop(&first), 0);
	assert_int_equal(bt_work_queue_drain(&second, true), 0);
	assert_int_equal(bt_work_queue_stop(&second), 0);
	assert_int_equal(bt_work_queue_deinit(&first), 0);
	assert_int_equal(bt_work_queue_deinit(&second), 0);
	context_deinit(&ctx);
}

static void test_self_stop(void **state)
{
	struct bt_work_q queue = {0};
	struct work_context ctx = {.stop_self = true};
	struct bt_work_sync sync;

	(void)state;
	assert_int_equal(bt_work_queue_init(&queue), 0);
	assert_int_equal(bt_work_queue_start(&queue, 0, 0, NULL), 0);
	context_init(&ctx, &queue);
	assert_int_equal(bt_work_submit_to_queue(&queue, &ctx.work), 1);
	assert_int_equal(osi_sem_take(&ctx.entered, 1000), 0);
	bt_work_flush(&ctx.work, &sync);
	assert_int_equal(ctx.stop_result, -EDEADLK);
	assert_true(queue.started);
	assert_int_equal(bt_work_queue_drain(&queue, true), 0);
	assert_int_equal(bt_work_queue_stop(&queue), 0);
	assert_int_equal(bt_work_queue_deinit(&queue), 0);
	context_deinit(&ctx);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_lifecycle),
		cmocka_unit_test(test_cross_queue),
		cmocka_unit_test(test_self_stop),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
