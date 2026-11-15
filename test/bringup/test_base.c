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
 * @brief Behavioral regression tests for private base mechanisms and retained net_buf.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <base/base.h>
#include <embt/net_buf.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/l2cap.h>
#include <host/gatt_internal.h>

struct item {
	sys_snode_t node;
	int value;
};
static bt_atomic_t counter;

static int setup(void **state)
{
	(void)state;
	return bt_base_init();
}

static int teardown(void **state)
{
	(void)state;
	return bt_base_deinit();
}

static void count_thread(void *arg)
{
	(void)arg;
	for (int i = 0; i < 10000; i++) {
		bt_atomic_inc(&counter);
	}
}

static void test_atomic(void **state)
{
	bt_atomic_t value = 3;
	BT_ATOMIC_DEFINE(bits, 130) = {0};
	void *pointer = &value;
	os_thread_t threads[4];

	(void)state;
	assert_int_equal(bt_atomic_add(&value, 2), 3);
	assert_int_equal(bt_atomic_sub(&value, 1), 5);
	assert_true(bt_atomic_cas(&value, 4, -1));
	assert_false(bt_atomic_cas(&value, 4, 0));
	assert_int_equal(bt_atomic_clear(&value), -1);
	assert_ptr_equal(bt_atomic_ptr_clear(&pointer), &value);
	assert_null(pointer);
	assert_false(bt_atomic_test_and_set_bit(bits, 129));
	assert_true(bt_atomic_test_bit(bits, 129));
	assert_true(bt_atomic_test_and_clear_bit(bits, 129));
	assert_false(bt_atomic_test_bit(bits, 129));
	for (size_t i = 0; i < ARRAY_SIZE(threads); i++) {
		assert_int_equal(
			osi_thread_create(&threads[i], count_thread, NULL, "atomic-test", 0, 0), 0);
	}
	for (size_t i = 0; i < ARRAY_SIZE(threads); i++) {
		assert_int_equal(osi_thread_join(&threads[i]), 0);
	}
	assert_int_equal(bt_atomic_get(&counter), 40000);
}

static void test_queue(void **state)
{
	struct k_queue queue;
	struct item a = {.value = 1}, b = {.value = 2}, c = {.value = 3};
	int64_t start;

	(void)state;
	k_queue_init(&queue);
	k_queue_append(&queue, &a);
	k_queue_append(&queue, &b);
	k_queue_prepend(&queue, &c);
	assert_ptr_equal(k_queue_peek_head(&queue), &c);
	assert_true(k_queue_remove(&queue, &b));
	assert_false(k_queue_remove(&queue, &b));
	assert_ptr_equal(k_queue_get(&queue, K_NO_WAIT), &c);
	assert_ptr_equal(k_queue_get(&queue, K_NO_WAIT), &a);
	start = k_uptime_get();
	assert_null(k_queue_get(&queue, K_MSEC(25)));
	assert_in_range(k_uptime_get() - start, 20, 500);
}

struct wait_context {
	struct k_queue queue;
	void *result;
	bt_atomic_t finished;
};

static void queue_wait(void *arg)
{
	struct wait_context *ctx = arg;

	ctx->result = k_queue_get(&ctx->queue, K_FOREVER);
	bt_atomic_set(&ctx->finished, 1);
}

static void test_queue_cancel(void **state)
{
	struct wait_context ctx = {0};
	struct item item = {0};
	os_thread_t thread;
	int64_t deadline;

	(void)state;
	k_queue_init(&ctx.queue);
	assert_int_equal(osi_thread_create(&thread, queue_wait, &ctx, "queue-wait", 0, 0), 0);
	deadline = k_uptime_get() + 1000;
	while (!bt_atomic_get(&ctx.finished) && k_uptime_get() < deadline) {
		k_queue_cancel_wait(&ctx.queue);
		osi_sleep_ms(1);
	}
	assert_true(bt_atomic_get(&ctx.finished));
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_null(ctx.result);
	bt_atomic_clear(&ctx.finished);
	assert_int_equal(osi_thread_create(&thread, queue_wait, &ctx, "queue-next", 0, 0), 0);
	k_queue_append(&ctx.queue, &item);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_ptr_equal(ctx.result, &item);
}

static void test_semaphore(void **state)
{
	struct k_sem sem;

	(void)state;
	assert_int_equal(k_sem_init(&sem, 0, 2), 0);
	k_sem_give(&sem);
	k_sem_give(&sem);
	k_sem_give(&sem);
	assert_int_equal(k_sem_count_get(&sem), 2);
	assert_int_equal(k_sem_take(&sem, K_NO_WAIT), 0);
	k_sem_reset(&sem);
	assert_int_equal(k_sem_take(&sem, K_MSEC(10)), -EAGAIN);
}

K_MEM_SLAB_DEFINE_STATIC(slab_storage, 32, 2, sizeof(void *));
static void test_slab(void **state)
{
	void *a, *b, *c;

	(void)state;
	bt_mem_slab_init(&slab_storage);
	assert_int_equal(k_mem_slab_alloc(&slab_storage, &a, K_NO_WAIT), 0);
	assert_int_equal(k_mem_slab_alloc(&slab_storage, &b, K_NO_WAIT), 0);
	assert_ptr_not_equal(a, b);
	assert_int_equal(k_mem_slab_alloc(&slab_storage, &c, K_NO_WAIT), -ENOMEM);
	k_mem_slab_free(&slab_storage, a);
	assert_int_equal(k_mem_slab_alloc(&slab_storage, &c, K_NO_WAIT), 0);
	assert_ptr_equal(c, a);
	k_mem_slab_free(&slab_storage, b);
	k_mem_slab_free(&slab_storage, c);
}

K_HEAP_DEFINE(arena, 1024);
static void test_arena(void **state)
{
	void *a, *b, *c;

	(void)state;
	a = k_heap_aligned_alloc(&arena, 64, 256, K_NO_WAIT);
	b = k_heap_aligned_alloc(&arena, 32, 256, K_NO_WAIT);
	assert_non_null(a);
	assert_non_null(b);
	assert_int_equal((uintptr_t)a % 64, 0);
	assert_int_equal((uintptr_t)b % 32, 0);
	memset(a, 0x5a, 256);
	memset(b, 0x3c, 256);
	assert_null(k_heap_alloc(&arena, 800, K_MSEC(10)));
	k_heap_free(&arena, a);
	k_heap_free(&arena, b);
	c = k_heap_alloc(&arena, 900, K_NO_WAIT);
	assert_non_null(c);
	k_heap_free(&arena, c);
}

NET_BUF_POOL_FIXED_DEFINE(buffers, 4, 64, 8, NULL);
NET_BUF_POOL_VAR_DEFINE(variable_buffers, 3, 1024, 0, NULL);

static void ref_thread(void *arg)
{
	struct net_buf *buf = arg;

	for (int i = 0; i < 10000; i++) {
		struct net_buf *held = net_buf_ref(buf);

		net_buf_unref(held);
	}
}

static void test_netbuf(void **state)
{
	struct net_buf *buf, *clone, *variable;
	os_thread_t threads[4];
	const char message[] = "EmBT buffer reference";

	(void)state;
	buf = net_buf_alloc(&buffers, K_NO_WAIT);
	assert_non_null(buf);
	assert_ptr_equal(buf->pool, &buffers);
	net_buf_add_mem(buf, message, sizeof(message));
	clone = net_buf_clone(buf, K_NO_WAIT);
	assert_non_null(clone);
	assert_ptr_equal(clone->pool, &buffers);
	assert_memory_equal(clone->data, message, sizeof(message));
	for (size_t i = 0; i < ARRAY_SIZE(threads); i++) {
		assert_int_equal(osi_thread_create(&threads[i], ref_thread, buf, "buf-ref", 0, 0),
				 0);
	}
	for (size_t i = 0; i < ARRAY_SIZE(threads); i++) {
		assert_int_equal(osi_thread_join(&threads[i]), 0);
	}
	assert_int_equal((unsigned char)bt_atomic_get(&buf->ref_word), 1);
	net_buf_unref(clone);
	net_buf_unref(buf);
	variable = net_buf_alloc_len(&variable_buffers, 256, K_NO_WAIT);
	assert_non_null(variable);
	net_buf_add_mem(variable, message, sizeof(message));
	clone = net_buf_clone(variable, K_NO_WAIT);
	assert_non_null(clone);
	assert_ptr_equal(clone->data, variable->data);
	net_buf_unref(variable);
	assert_memory_equal(clone->data, message, sizeof(message));
	net_buf_unref(clone);
}

struct work_context {
	struct k_work_delayable work;
	struct k_sem entered;
	struct k_sem release;
	bt_atomic_t count;
	bool block;
};

static void work_handler(struct k_work *work)
{
	struct work_context *ctx =
		CONTAINER_OF(k_work_delayable_from_work(work), struct work_context, work);

	bt_atomic_inc(&ctx->count);
	k_sem_give(&ctx->entered);
	if (ctx->block) {
		k_sem_take(&ctx->release, K_FOREVER);
	}
}

static void init_work_context(struct work_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	k_sem_init(&ctx->entered, 0, 4);
	k_sem_init(&ctx->release, 0, 1);
	k_work_init_delayable(&ctx->work, work_handler);
}

static void test_work(void **state)
{
	struct work_context ctx;
	struct k_work_sync sync;
	int64_t start;

	(void)state;
	init_work_context(&ctx);
	start = k_uptime_get();
	assert_int_equal(k_work_schedule(&ctx.work, K_MSEC(200)), 1);
	assert_int_equal(k_work_reschedule(&ctx.work, K_MSEC(25)), 1);
	assert_int_equal(k_sem_take(&ctx.entered, K_MSEC(1000)), 0);
	assert_in_range(k_uptime_get() - start, 20, 500);
	k_work_flush(&ctx.work.work, &sync);
	assert_int_equal(bt_atomic_get(&ctx.count), 1);
	assert_int_equal(k_work_schedule(&ctx.work, K_MSEC(100)), 1);
	assert_int_equal(k_work_cancel_delayable(&ctx.work), 0);
	assert_int_equal(k_sem_take(&ctx.entered, K_MSEC(150)), -EAGAIN);
}

struct cancel_context {
	struct work_context *work;
	bt_atomic_t done;
};
static void cancel_thread(void *arg)
{
	struct cancel_context *ctx = arg;
	struct k_work_sync sync;

	k_work_cancel_delayable_sync(&ctx->work->work, &sync);
	bt_atomic_set(&ctx->done, 1);
}

static void test_work_cancel(void **state)
{
	struct work_context ctx;
	struct cancel_context cancel = {.work = &ctx};
	os_thread_t thread;
	int64_t deadline;

	(void)state;
	init_work_context(&ctx);
	ctx.block = true;
	assert_int_equal(k_work_submit(&ctx.work.work), 1);
	assert_int_equal(k_sem_take(&ctx.entered, K_MSEC(1000)), 0);
	assert_int_equal(osi_thread_create(&thread, cancel_thread, &cancel, "work-cancel", 0, 0),
			 0);
	deadline = k_uptime_get() + 1000;
	while (!(k_work_busy_get(&ctx.work.work) & K_WORK_CANCELING) && k_uptime_get() < deadline) {
		osi_sleep_ms(1);
	}
	assert_false(bt_atomic_get(&cancel.done));
	assert_int_equal(k_work_submit(&ctx.work.work), -EBUSY);
	k_sem_give(&ctx.release);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_true(bt_atomic_get(&cancel.done));
	assert_int_equal(k_work_busy_get(&ctx.work.work), 0);
}

static struct k_sem freed;
static void self_free(struct k_work *work)
{
	osi_free(work);
	k_sem_give(&freed);
}

static void test_work_lifetime(void **state)
{
	struct k_work *work = osi_malloc(sizeof(*work));

	(void)state;
	assert_non_null(work);
	k_sem_init(&freed, 0, 1);
	k_work_init(work, self_free);
	assert_int_equal(k_work_submit(work), 1);
	assert_int_equal(k_sem_take(&freed, K_MSEC(1000)), 0);
	/* Teardown joins the worker; ASan catches a post-handler container access. */
}

static void test_work_async_cancel(void **state)
{
	struct work_context ctx;
	struct k_work_sync sync;

	(void)state;
	init_work_context(&ctx);
	ctx.block = true;
	assert_int_equal(k_work_submit(&ctx.work.work), 1);
	assert_int_equal(k_sem_take(&ctx.entered, K_MSEC(1000)), 0);
	assert_true(k_work_cancel(&ctx.work.work) & K_WORK_RUNNING);
	assert_int_equal(k_work_submit(&ctx.work.work), -EBUSY);
	assert_int_equal(k_work_schedule(&ctx.work, K_MSEC(10)), -EBUSY);
	k_sem_give(&ctx.release);
	k_work_flush(&ctx.work.work, &sync);
	ctx.block = false;
	assert_int_equal(k_work_submit(&ctx.work.work), 1);
	assert_int_equal(k_sem_take(&ctx.entered, K_MSEC(1000)), 0);
	k_work_flush(&ctx.work.work, &sync);
	assert_int_equal(bt_atomic_get(&ctx.count), 2);
}

static void test_deadline_lifetime(void **state)
{
	struct work_context *ctx = osi_malloc(sizeof(*ctx));
	struct k_work_sync sync;

	(void)state;
	assert_non_null(ctx);
	init_work_context(ctx);
	assert_int_equal(k_work_schedule(&ctx->work, K_MSEC(25)), 1);
	assert_true(k_work_cancel_delayable_sync(&ctx->work, &sync));
	osi_free(ctx);
	osi_sleep_ms(40);
}

static void test_pool_reuse(void **state)
{
	struct net_buf *allocated[4];

	(void)state;
	for (int cycle = 0; cycle < 5; cycle++) {
		for (size_t i = 0; i < ARRAY_SIZE(allocated); i++) {
			allocated[i] = net_buf_alloc(&buffers, K_NO_WAIT);
			assert_non_null(allocated[i]);
			for (size_t j = 0; j < i; j++) {
				assert_ptr_not_equal(allocated[i], allocated[j]);
			}
		}
		assert_null(net_buf_alloc(&buffers, K_NO_WAIT));
		for (size_t i = 0; i < ARRAY_SIZE(allocated); i++) {
			net_buf_unref(allocated[i]);
		}
	}
}

BT_GATT_SERVICE_DEFINE(test_service, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0xfff0)));
static int accept_test_channel(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	(void)conn;
	(void)chan;
	return -ENOMEM;
}

static void test_registries(void **state)
{
	static const struct bt_l2cap_fixed_chan channel = {.cid = 0x3e,
							   .accept = accept_test_channel};
	static const struct bt_l2cap_fixed_chan duplicate = {.cid = 0x3e,
							     .accept = accept_test_channel};
	struct bt_l2cap_chan chan = {0};

	(void)state;
	assert_int_equal(bt_stack_init(), 0);
	assert_int_equal(bt_stack_init(), 0);
	assert_int_equal(bt_l2cap_fixed_chan_register(&channel), 0);
	assert_int_equal(bt_l2cap_fixed_chan_register(&channel), 0);
	assert_int_equal(bt_l2cap_fixed_chan_register(&duplicate), -EADDRINUSE);
	assert_int_equal(bt_gatt_service_static_register(&test_service), 0);
	assert_int_equal(bt_gatt_service_static_register(&test_service), -EALREADY);
	bt_gatt_init();
	assert_true(bt_gatt_attr_get_handle(&test_service.attrs[0]) > 1);
	bt_atomic_set_bit(chan.status, BT_L2CAP_STATUS_OUT);
	assert_int_equal(bt_l2cap_chan_status_get(&chan), BIT(BT_L2CAP_STATUS_OUT));
}

struct named_test {
	const char *name;
	void (*fn)(void **);
};
static const struct named_test cases[] = {
	{"atomic", test_atomic},
	{"queue", test_queue},
	{"queue_cancel", test_queue_cancel},
	{"semaphore", test_semaphore},
	{"slab", test_slab},
	{"arena", test_arena},
	{"netbuf", test_netbuf},
	{"work", test_work},
	{"work_cancel", test_work_cancel},
	{"work_lifetime", test_work_lifetime},
	{"work_async_cancel", test_work_async_cancel},
	{"deadline_lifetime", test_deadline_lifetime},
	{"pool_reuse", test_pool_reuse},
	{"registries", test_registries},
};

int main(int argc, char **argv)
{
	if (argc != 2) {
		return 2;
	}
	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		if (!strcmp(argv[1], cases[i].name)) {
			const struct CMUnitTest test[] = {
				{cases[i].name, cases[i].fn, setup, teardown, NULL}};

			return cmocka_run_group_tests(test, NULL, NULL);
		}
	}
	return 2;
}
