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

/** @file @brief Queue isolation, native lifetime and dynamic buffer allocation tests. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <cmocka.h>
#include <embt/base/queue.h>
#include <embt/base/atomic.h>
#include <embt/net_buf.h>
#include <embt/osi/osi.h>

int __real_osi_sem_init(os_sem_t *sem, uint32_t count, uint32_t limit);
void *__real_osi_malloc(size_t size);
void *__real_osi_calloc(size_t count, size_t size);
void *__real_osi_aligned_alloc(size_t alignment, size_t size);
void __real_osi_free(void *ptr);

static _Thread_local bool pause_wait_init;
static os_sem_t paused;
static os_sem_t resume;
static bool fail_malloc;
static bool fail_calloc;
static bool fail_aligned;
static bool track_allocation;
static void *payload_allocation;
static unsigned int payload_frees;

int __wrap_osi_sem_init(os_sem_t *sem, uint32_t count, uint32_t limit)
{
	if (pause_wait_init) {
		pause_wait_init = false;
		osi_sem_give(&paused);
		osi_sem_take(&resume, OS_TIMEOUT_FOREVER);
	}
	return __real_osi_sem_init(sem, count, limit);
}

void *__wrap_osi_malloc(size_t size)
{
	void *result = fail_malloc ? NULL : __real_osi_malloc(size);

	if (track_allocation) {
		payload_allocation = result;
	}
	return result;
}

void *__wrap_osi_calloc(size_t count, size_t size)
{
	return fail_calloc ? NULL : __real_osi_calloc(count, size);
}

void *__wrap_osi_aligned_alloc(size_t alignment, size_t size)
{
	void *result = fail_aligned ? NULL : __real_osi_aligned_alloc(alignment, size);

	if (track_allocation) {
		payload_allocation = result;
	}
	return result;
}

void __wrap_osi_free(void *ptr)
{
	if (ptr != NULL && ptr == payload_allocation) {
		payload_frees++;
		payload_allocation = NULL;
	}
	__real_osi_free(ptr);
}

struct item {
	sys_snode_t node;
	unsigned int value;
};

struct queue_context {
	struct bt_queue *queue;
	struct item *item;
	os_sem_t *done;
	os_sem_t *start;
	void *received;
};

static void paused_consumer(void *arg)
{
	struct queue_context *ctx = arg;

	pause_wait_init = true;
	ctx->received = bt_queue_get(ctx->queue, BT_MSEC(20));
}

static void producer(void *arg)
{
	struct queue_context *ctx = arg;

	if (ctx->start) {
		osi_sem_take(ctx->start, OS_TIMEOUT_FOREVER);
	}
	bt_queue_append(ctx->queue, ctx->item);
	if (ctx->done) {
		osi_sem_give(ctx->done);
	}
}

static void test_queue_isolation(void **state)
{
	struct bt_queue first = {0}, second = {0};
	struct item item = {.value = 42};
	os_sem_t done;
	os_thread_t consumer, writer;
	struct queue_context get = {.queue = &first};
	struct queue_context put = {.queue = &second, .item = &item, .done = &done};
	int progressed;

	(void)state;
	assert_int_equal(osi_sem_init(&paused, 0, 1), 0);
	assert_int_equal(osi_sem_init(&resume, 0, 1), 0);
	assert_int_equal(osi_sem_init(&done, 0, 1), 0);
	bt_queue_init(&first);
	bt_queue_init(&second);
	assert_ptr_not_equal(&first.mutex, &second.mutex);
	assert_int_equal(osi_thread_create(&consumer, paused_consumer, &get, NULL, 0, 0), 0);
	assert_int_equal(osi_sem_take(&paused, 1000), 0);
	/* The first queue is paused while holding its actual native mutex. */
	assert_int_equal(osi_thread_create(&writer, producer, &put, NULL, 0, 0), 0);
	progressed = osi_sem_take(&done, 500);
	osi_sem_give(&resume);
	assert_int_equal(osi_thread_join(&consumer), 0);
	assert_int_equal(osi_thread_join(&writer), 0);
	assert_int_equal(progressed, 0);
	assert_ptr_equal(bt_queue_get(&second, BT_NO_WAIT), &item);
	assert_null(get.received);
	assert_int_equal(bt_queue_deinit(&first), 0);
	assert_int_equal(bt_queue_deinit(&second), 0);
	assert_int_equal(osi_sem_deinit(&paused), 0);
	assert_int_equal(osi_sem_deinit(&resume), 0);
	assert_int_equal(osi_sem_deinit(&done), 0);
}

static void test_queue_without_heap(void **state)
{
	struct bt_queue queue = {0};
	struct item item = {0};

	(void)state;
	fail_malloc = true;
	fail_calloc = true;
	bt_queue_init(&queue);
	assert_true(queue.initialized);
	assert_int_equal(osi_mutex_lock(&queue.mutex, OS_TIMEOUT_NO_WAIT), 0);
	assert_int_equal(osi_mutex_unlock(&queue.mutex), 0);
	bt_queue_append(&queue, &item);
	assert_ptr_equal(bt_queue_get(&queue, BT_NO_WAIT), &item);
	assert_int_equal(bt_queue_deinit(&queue), 0);
	fail_malloc = false;
	fail_calloc = false;
}

static void test_queue_requires_init(void **state)
{
	pid_t child;
	int status;

	(void)state;
	child = fork();
	assert_true(child >= 0);
	if (child == 0) {
		struct rlimit limit = {0, 0};
		struct bt_queue queue = {0};
		struct item item = {0};

		(void)setrlimit(RLIMIT_CORE, &limit);
		bt_queue_append(&queue, &item);
		_exit(99);
	}
	assert_int_equal(waitpid(child, &status, 0), child);
	assert_true(WIFSIGNALED(status));
	assert_int_equal(WTERMSIG(status), SIGABRT);
}

static void test_queue_reuse(void **state)
{
	struct bt_queue queue = {0};
	struct item item = {0};

	(void)state;
	for (int i = 0; i < 100; i++) {
		bt_queue_init(&queue);
		os_mutex_t *mutex = &queue.mutex;

		bt_queue_append(&queue, &item);
		assert_ptr_equal(bt_queue_get(&queue, BT_NO_WAIT), &item);
		bt_queue_init(&queue);
		assert_ptr_equal(&queue.mutex, mutex);
		assert_int_equal(bt_queue_deinit(&queue), 0);
		assert_false(queue.initialized);
		assert_int_equal(bt_queue_deinit(&queue), 0);
	}
}

static void test_queue_concurrent_use(void **state)
{
	struct bt_queue queue = {0};
	struct item items[16] = {0};
	os_thread_t threads[16];
	struct queue_context ctx[16] = {0};
	os_sem_t start;
	unsigned int seen = 0;

	(void)state;
	bt_queue_init(&queue);
	assert_int_equal(osi_sem_init(&start, 0, 16), 0);
	for (unsigned int i = 0; i < 16; i++) {
		items[i].value = i;
		ctx[i].queue = &queue;
		ctx[i].item = &items[i];
		ctx[i].start = &start;
		assert_int_equal(osi_thread_create(&threads[i], producer, &ctx[i], NULL, 0, 0), 0);
	}
	for (unsigned int i = 0; i < 16; i++) {
		osi_sem_give(&start);
	}
	for (unsigned int i = 0; i < 16; i++) {
		assert_int_equal(osi_thread_join(&threads[i]), 0);
	}
	for (unsigned int i = 0; i < 16; i++) {
		struct item *entry = bt_queue_get(&queue, BT_NO_WAIT);

		assert_non_null(entry);
		assert_true(entry->value < 16);
		assert_false(seen & (1U << entry->value));
		seen |= 1U << entry->value;
	}
	assert_int_equal(seen, 0xffff);
	assert_null(bt_queue_get(&queue, BT_NO_WAIT));
	assert_int_equal(bt_queue_deinit(&queue), 0);
	assert_int_equal(osi_sem_deinit(&start), 0);
}

static void waiting_consumer(void *arg)
{
	struct queue_context *ctx = arg;

	pause_wait_init = true;
	ctx->received = bt_queue_get(ctx->queue, BT_FOREVER);
}

static void test_queue_waiter_lifetime(void **state)
{
	struct bt_queue queue = {0};
	struct queue_context ctx = {.queue = &queue};
	os_thread_t thread;

	(void)state;
	assert_int_equal(osi_sem_init(&paused, 0, 1), 0);
	assert_int_equal(osi_sem_init(&resume, 0, 1), 0);
	bt_queue_init(&queue);
	assert_int_equal(osi_thread_create(&thread, waiting_consumer, &ctx, NULL, 0, 0), 0);
	assert_int_equal(osi_sem_take(&paused, 1000), 0);
	osi_sem_give(&resume);
	/* Acquiring the mutex proves the waiter has registered and released it. */
	assert_null(bt_queue_peek_head(&queue));
	assert_int_equal(bt_queue_deinit(&queue), -EBUSY);
	bt_queue_cancel_wait(&queue);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_null(ctx.received);
	assert_int_equal(bt_queue_deinit(&queue), 0);
	assert_int_equal(osi_sem_deinit(&paused), 0);
	assert_int_equal(osi_sem_deinit(&resume), 0);
}

NET_BUF_POOL_HEAP_DEFINE(dynamic_pool, 3, 0, NULL);
NET_BUF_POOL_HEAP_ALIGN_DEFINE(aligned_pool, 3, 0, NULL, 64);

static void check_payload(struct net_buf_pool *pool, size_t alignment)
{
	struct net_buf *buf, *clone;

	track_allocation = true;
	payload_frees = 0;
	buf = net_buf_alloc_len(pool, 3, BT_FOREVER);
	track_allocation = false;
	assert_non_null(buf);
	assert_non_null(payload_allocation);
	assert_int_equal((uintptr_t)buf->data % alignment, 0);
	net_buf_add_mem(buf, "abc", 3);
	clone = net_buf_clone(buf, BT_NO_WAIT);
	assert_non_null(clone);
	assert_ptr_equal(clone->data, buf->data);
	net_buf_unref(buf);
	assert_int_equal(payload_frees, 0);
	assert_memory_equal(clone->data, "abc", 3);
	net_buf_unref(clone);
	assert_int_equal(payload_frees, 1);
}

static void test_dynamic_payload(void **state)
{
	(void)state;
	check_payload(&dynamic_pool, sizeof(void *));
	check_payload(&aligned_pool, 64);
}

static void check_allocation_abort(int kind)
{
	pid_t child = fork();
	int status;

	assert_true(child >= 0);
	if (child == 0) {
		struct rlimit limit = {0, 0};
		struct net_buf *unexpected;

		(void)setrlimit(RLIMIT_CORE, &limit);
		fail_malloc = kind == 0;
		fail_aligned = kind == 1;
		unexpected = net_buf_alloc_len(kind == 1 ? &aligned_pool : &dynamic_pool,
					       kind == 2 ? SIZE_MAX : 64, BT_FOREVER);
		(void)unexpected;
		_exit(99);
	}
	assert_int_equal(waitpid(child, &status, 0), child);
	assert_true(WIFSIGNALED(status));
	assert_int_equal(WTERMSIG(status), SIGABRT);
}

static void test_dynamic_failure(void **state)
{
	(void)state;
	check_allocation_abort(0);
	check_allocation_abort(1);
	check_allocation_abort(2);
}

int main(void)
{
	net_buf_pool_init(&dynamic_pool);
	net_buf_pool_init(&aligned_pool);
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_queue_isolation),
		cmocka_unit_test(test_queue_without_heap),
		cmocka_unit_test(test_queue_requires_init),
		cmocka_unit_test(test_queue_reuse),
		cmocka_unit_test(test_queue_concurrent_use),
		cmocka_unit_test(test_queue_waiter_lifetime),
		cmocka_unit_test(test_dynamic_payload),
		cmocka_unit_test(test_dynamic_failure),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
