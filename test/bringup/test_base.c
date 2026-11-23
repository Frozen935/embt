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

#include <embt/base/queue.h>
#include <embt/base/mem_slab.h>
#include <embt/base/work.h>
#include <embt/base/time.h>
#include <embt/base/time.h>
#include <embt/base/base.h>
#include <embt/base/atomic.h>
#include <embt/osi/sync.h>
#include <embt/osi/thread.h>
#include <embt/osi/time.h>
#include <embt/osi/memory.h>
#include <string.h>
#include <embt/base/assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
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
static void init_test_pools(void);

static int setup(void **state)
{
	(void)state;
	init_test_pools();
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
	struct bt_queue queue = {0};
	struct item a = {.value = 1}, b = {.value = 2}, c = {.value = 3};
	int64_t start;

	(void)state;
	bt_queue_init(&queue);
	bt_queue_append(&queue, &a);
	bt_queue_append(&queue, &b);
	bt_queue_prepend(&queue, &c);
	assert_ptr_equal(bt_queue_peek_head(&queue), &c);
	assert_true(bt_queue_remove(&queue, &b));
	assert_false(bt_queue_remove(&queue, &b));
	assert_ptr_equal(bt_queue_get(&queue, BT_NO_WAIT), &c);
	assert_ptr_equal(bt_queue_get(&queue, BT_NO_WAIT), &a);
	start = bt_uptime_get_ms();
	assert_null(bt_queue_get(&queue, BT_MSEC(25)));
	assert_in_range(bt_uptime_get_ms() - start, 20, 500);
	assert_int_equal(bt_queue_deinit(&queue), 0);
}

struct wait_context {
	struct bt_queue queue;
	void *result;
	bt_atomic_t finished;
};

static void queue_wait(void *arg)
{
	struct wait_context *ctx = arg;

	ctx->result = bt_queue_get(&ctx->queue, BT_FOREVER);
	bt_atomic_set(&ctx->finished, 1);
}

static void test_queue_cancel(void **state)
{
	struct wait_context ctx = {0};
	struct item item = {0};
	os_thread_t thread;
	int64_t deadline;

	(void)state;
	bt_queue_init(&ctx.queue);
	assert_int_equal(osi_thread_create(&thread, queue_wait, &ctx, "queue-wait", 0, 0), 0);
	deadline = bt_uptime_get_ms() + 1000;
	while (!bt_atomic_get(&ctx.finished) && bt_uptime_get_ms() < deadline) {
		bt_queue_cancel_wait(&ctx.queue);
		osi_sleep_ms(1);
	}
	assert_true(bt_atomic_get(&ctx.finished));
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_null(ctx.result);
	bt_atomic_clear(&ctx.finished);
	assert_int_equal(osi_thread_create(&thread, queue_wait, &ctx, "queue-next", 0, 0), 0);
	bt_queue_append(&ctx.queue, &item);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_ptr_equal(ctx.result, &item);
	assert_int_equal(bt_queue_deinit(&ctx.queue), 0);
}

static void test_semaphore(void **state)
{
	os_sem_t sem;
	uint32_t available;

	(void)state;
	assert_int_equal(osi_sem_init(&sem, 0, 2), 0);
	osi_sem_give(&sem);
	osi_sem_give(&sem);
	osi_sem_give(&sem);
	assert_int_equal(osi_sem_count_get(&sem, &available), 0);
	assert_int_equal(available, 2);
	assert_int_equal(osi_sem_take(&sem, BT_NO_WAIT), 0);
	osi_sem_reset(&sem);
	assert_int_equal(osi_sem_take(&sem, BT_MSEC(10)), -ETIMEDOUT);
	assert_int_equal(osi_sem_deinit(&sem), 0);
}

BT_MEM_SLAB_DEFINE_STATIC(slab_storage, 32, 2, sizeof(void *));
static void test_slab(void **state)
{
	void *a, *b, *c;

	(void)state;
	bt_mem_slab_init(&slab_storage);
	assert_int_equal(bt_mem_slab_alloc(&slab_storage, &a, BT_NO_WAIT), 0);
	assert_int_equal(bt_mem_slab_alloc(&slab_storage, &b, BT_NO_WAIT), 0);
	assert_ptr_not_equal(a, b);
	assert_int_equal(bt_mem_slab_alloc(&slab_storage, &c, BT_NO_WAIT), -ENOMEM);
	bt_mem_slab_free(&slab_storage, a);
	assert_int_equal(bt_mem_slab_alloc(&slab_storage, &c, BT_NO_WAIT), 0);
	assert_ptr_equal(c, a);
	bt_mem_slab_free(&slab_storage, b);
	bt_mem_slab_free(&slab_storage, c);
}

NET_BUF_POOL_FIXED_DEFINE(buffers, 4, 64, 8, NULL);
NET_BUF_POOL_HEAP_DEFINE(variable_buffers, 3, 0, NULL);

static void init_test_pools(void)
{
	net_buf_pool_init(&buffers);
	net_buf_pool_init(&variable_buffers);
}


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
	buf = net_buf_alloc(&buffers, BT_NO_WAIT);
	assert_non_null(buf);
	assert_ptr_equal(buf->pool, &buffers);
	net_buf_add_mem(buf, message, sizeof(message));
	clone = net_buf_clone(buf, BT_NO_WAIT);
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
	variable = net_buf_alloc_len(&variable_buffers, 256, BT_NO_WAIT);
	assert_non_null(variable);
	net_buf_add_mem(variable, message, sizeof(message));
	clone = net_buf_clone(variable, BT_NO_WAIT);
	assert_non_null(clone);
	assert_ptr_equal(clone->data, variable->data);
	net_buf_unref(variable);
	assert_memory_equal(clone->data, message, sizeof(message));
	net_buf_unref(clone);
}

struct work_context {
	struct bt_work_delayable work;
	os_sem_t entered;
	os_sem_t release;
	bt_atomic_t count;
	bool block;
};

static void work_handler(struct bt_work *work)
{
	struct work_context *ctx =
		CONTAINER_OF(bt_work_delayable_from_work(work), struct work_context, work);

	bt_atomic_inc(&ctx->count);
	osi_sem_give(&ctx->entered);
	if (ctx->block) {
		osi_sem_take(&ctx->release, BT_FOREVER);
	}
}

static void init_work_context(struct work_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	osi_sem_init(&ctx->entered, 0, 4);
	osi_sem_init(&ctx->release, 0, 1);
	bt_work_init_delayable(&ctx->work, work_handler);
}

static void test_work(void **state)
{
	struct work_context ctx;
	struct bt_work_sync sync;
	int64_t start;

	(void)state;
	init_work_context(&ctx);
	start = bt_uptime_get_ms();
	assert_int_equal(bt_work_schedule(&ctx.work, BT_MSEC(200)), 1);
	assert_int_equal(bt_work_reschedule(&ctx.work, BT_MSEC(25)), 1);
	assert_int_equal(osi_sem_take(&ctx.entered, BT_MSEC(1000)), 0);
	assert_in_range(bt_uptime_get_ms() - start, 20, 500);
	bt_work_flush_delayable(&ctx.work, &sync);
	assert_int_equal(bt_atomic_get(&ctx.count), 1);
	assert_int_equal(bt_work_schedule(&ctx.work, BT_MSEC(100)), 1);
	assert_int_equal(bt_work_cancel_delayable(&ctx.work), 0);
	assert_int_equal(osi_sem_take(&ctx.entered, BT_MSEC(150)), -ETIMEDOUT);
	assert_int_equal(osi_sem_deinit(&ctx.entered), 0);
	assert_int_equal(osi_sem_deinit(&ctx.release), 0);
}

struct cancel_context {
	struct work_context *work;
	bt_atomic_t done;
};
static void cancel_thread(void *arg)
{
	struct cancel_context *ctx = arg;
	struct bt_work_sync sync;

	bt_work_cancel_delayable_sync(&ctx->work->work, &sync);
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
	assert_int_equal(bt_work_submit(&ctx.work.work), 1);
	assert_int_equal(osi_sem_take(&ctx.entered, BT_MSEC(1000)), 0);
	assert_int_equal(osi_thread_create(&thread, cancel_thread, &cancel, "work-cancel", 0, 0),
			 0);
	deadline = bt_uptime_get_ms() + 1000;
	while (!(bt_work_busy_get(&ctx.work.work) & BT_WORK_CANCELING) && bt_uptime_get_ms() < deadline) {
		osi_sleep_ms(1);
	}
	assert_false(bt_atomic_get(&cancel.done));
	assert_int_equal(bt_work_submit(&ctx.work.work), -EBUSY);
	osi_sem_give(&ctx.release);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_true(bt_atomic_get(&cancel.done));
	assert_int_equal(bt_work_busy_get(&ctx.work.work), 0);
	assert_int_equal(osi_sem_deinit(&ctx.entered), 0);
	assert_int_equal(osi_sem_deinit(&ctx.release), 0);
}

static os_sem_t freed;
static void lifetime_handler(struct bt_work *work)
{
	(void)work;
	osi_sem_give(&freed);
}

static void test_work_lifetime(void **state)
{
	struct bt_work *work = osi_malloc(sizeof(*work));

	(void)state;
	assert_non_null(work);
	osi_sem_init(&freed, 0, 1);
	bt_work_init(work, lifetime_handler);
	assert_int_equal(bt_work_submit(work), 1);
	assert_int_equal(osi_sem_take(&freed, BT_MSEC(1000)), 0);
	/* Native work storage must outlive post-handler flag updates and flushing. */
	struct bt_work_sync sync;
	bt_work_flush(work, &sync);
	osi_free(work);
	assert_int_equal(bt_base_deinit(), 0);
	assert_int_equal(osi_sem_deinit(&freed), 0);
}

static void test_work_async_cancel(void **state)
{
	struct work_context ctx;
	struct bt_work_sync sync;

	(void)state;
	init_work_context(&ctx);
	ctx.block = true;
	assert_int_equal(bt_work_submit(&ctx.work.work), 1);
	assert_int_equal(osi_sem_take(&ctx.entered, BT_MSEC(1000)), 0);
	assert_true(bt_work_cancel_delayable(&ctx.work) & BT_WORK_RUNNING);
	assert_int_equal(bt_work_submit(&ctx.work.work), -EBUSY);
	assert_int_equal(bt_work_schedule(&ctx.work, BT_MSEC(10)), 0);
	osi_sem_give(&ctx.release);
	bt_work_flush_delayable(&ctx.work, &sync);
	ctx.block = false;
	assert_int_equal(bt_work_submit(&ctx.work.work), 1);
	assert_int_equal(osi_sem_take(&ctx.entered, BT_MSEC(1000)), 0);
	bt_work_flush_delayable(&ctx.work, &sync);
	assert_int_equal(bt_atomic_get(&ctx.count), 2);
	assert_int_equal(osi_sem_deinit(&ctx.entered), 0);
	assert_int_equal(osi_sem_deinit(&ctx.release), 0);
}

static void test_deadline_lifetime(void **state)
{
	struct work_context *ctx = osi_malloc(sizeof(*ctx));
	struct bt_work_sync sync;

	(void)state;
	assert_non_null(ctx);
	init_work_context(ctx);
	assert_int_equal(bt_work_schedule(&ctx->work, BT_MSEC(25)), 1);
	assert_true(bt_work_cancel_delayable_sync(&ctx->work, &sync));
	assert_int_equal(osi_sem_deinit(&ctx->entered), 0);
	assert_int_equal(osi_sem_deinit(&ctx->release), 0);
	osi_free(ctx);
	osi_sleep_ms(40);
}

static void test_pool_reuse(void **state)
{
	struct net_buf *allocated[4];

	(void)state;
	for (int cycle = 0; cycle < 5; cycle++) {
		for (size_t i = 0; i < ARRAY_SIZE(allocated); i++) {
			allocated[i] = net_buf_alloc(&buffers, BT_NO_WAIT);
			assert_non_null(allocated[i]);
			for (size_t j = 0; j < i; j++) {
				assert_ptr_not_equal(allocated[i], allocated[j]);
			}
		}
		assert_null(net_buf_alloc(&buffers, BT_NO_WAIT));
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

static void test_time_model(void **state)
{
	bt_timepoint_t deadline;
	bt_timeout_t remaining;
	int64_t began;
	uint64_t microseconds;

	(void)state;
	assert_int_equal(sizeof(bt_timeout_t), sizeof(int64_t));
	assert_int_equal(sizeof(bt_timepoint_t), sizeof(int64_t));
	assert_int_equal(BT_MSEC(7), 7);
	assert_int_equal(BT_SECONDS(2), 2000);
	assert_int_equal(BT_NO_WAIT, 0);
	assert_int_equal(BT_FOREVER, -1);
	assert_int_equal(bt_time_us_to_ms_ceil(0), 0);
	assert_int_equal(bt_time_us_to_ms_ceil(1), 1);
	assert_int_equal(bt_time_us_to_ms_ceil(999), 1);
	assert_int_equal(bt_time_us_to_ms_ceil(1000), 1);
	assert_int_equal(bt_time_us_to_ms_ceil(1001), 2);
	assert_true(bt_time_us_to_ms_ceil(UINT64_MAX) <= INT64_MAX);
	began = bt_uptime_get_ms();
	deadline = bt_timepoint_calc(BT_MSEC(250));
	assert_true(deadline.deadline_ms >= began + 250);
	remaining = bt_timepoint_timeout(deadline);
	assert_true(remaining >= 0 && remaining <= 250);
	deadline = bt_timepoint_calc(BT_FOREVER);
	assert_int_equal(deadline.deadline_ms, INT64_MAX);
	assert_int_equal(bt_timepoint_timeout(deadline), -1);
	assert_false(bt_timepoint_expired(deadline));
	deadline = (bt_timepoint_t){.deadline_ms = 0};
	assert_true(bt_timepoint_expired(deadline));
	assert_int_equal(bt_timepoint_timeout(deadline), 0);
	deadline = bt_timepoint_calc(BT_MSEC(INT64_MAX));
	assert_int_equal(deadline.deadline_ms, INT64_MAX);
	microseconds = bt_uptime_get_us();
	assert_true(microseconds / 1000 <= (uint64_t)bt_uptime_get_ms());
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
	{"netbuf", test_netbuf},
	{"work", test_work},
	{"work_cancel", test_work_cancel},
	{"work_lifetime", test_work_lifetime},
	{"work_async_cancel", test_work_async_cancel},
	{"deadline_lifetime", test_deadline_lifetime},
	{"pool_reuse", test_pool_reuse},
	{"registries", test_registries},
	{"time_model", test_time_model},
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
