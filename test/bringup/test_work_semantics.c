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

/** @file @brief Zephyr work contracts with semaphore-only OSI synchronization. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <embt/base/work.h>
#include <embt/base/atomic.h>
#include <embt/osi/osi.h>

/* Work must not depend on an OS condition variable. */
int __wrap_osi_cond_init(os_cond_t *cond)
{
	(void)cond;
	return -ENOTSUP;
}
int __wrap_osi_cond_wait(os_cond_t *cond, os_mutex_t *mutex, os_timeout_t timeout)
{
	(void)cond;
	(void)mutex;
	(void)timeout;
	osi_fatal("Work attempted a condition wait");
}

struct fixture {
	struct bt_work_q first;
	struct bt_work_q second;
	struct bt_work gate;
	struct bt_work immediate;
	struct bt_work barrier;
	struct bt_work_delayable delayed;
	os_sem_t entered;
	os_sem_t release;
	os_sem_t done;
	unsigned int order[4];
	unsigned int count;
	unsigned int hits;
};
static struct fixture f;

static void gate_handler(struct bt_work *work)
{
	(void)work;
	osi_sem_give(&f.entered);
	osi_sem_take(&f.release, OS_TIMEOUT_FOREVER);
}
static void delayed_handler(struct bt_work *work)
{
	(void)work;
	f.hits++;
	f.order[f.count++] = 1;
	osi_sem_give(&f.done);
}
static void immediate_handler(struct bt_work *work)
{
	(void)work;
	f.order[f.count++] = 2;
	osi_sem_give(&f.done);
}
static void barrier_handler(struct bt_work *work)
{
	(void)work;
	osi_sem_give(&f.done);
}
static int setup(void **state)
{
	(void)state;
	memset(&f, 0, sizeof(f));
	assert_int_equal(osi_sem_init(&f.entered, 0, 1), 0);
	assert_int_equal(osi_sem_init(&f.release, 0, 4), 0);
	assert_int_equal(osi_sem_init(&f.done, 0, 4), 0);
	assert_int_equal(bt_work_queue_init(&f.first), 0);
	assert_int_equal(bt_work_queue_init(&f.second), 0);
	assert_int_equal(bt_work_queue_start(&f.first, 0, 0, NULL), 0);
	assert_int_equal(bt_work_queue_start(&f.second, 0, 0, NULL), 0);
	bt_work_init(&f.gate, gate_handler);
	bt_work_init(&f.immediate, immediate_handler);
	bt_work_init(&f.barrier, barrier_handler);
	bt_work_init_delayable(&f.delayed, delayed_handler);
	return 0;
}
static int teardown(void **state)
{
	struct bt_work_sync sync;
	(void)state;
	bt_work_cancel_delayable_sync(&f.delayed, &sync);
	assert_int_equal(bt_work_queue_drain(&f.first, true), 0);
	assert_int_equal(bt_work_queue_drain(&f.second, true), 0);
	assert_int_equal(bt_work_queue_stop(&f.first), 0);
	assert_int_equal(bt_work_queue_stop(&f.second), 0);
	assert_int_equal(bt_work_queue_deinit(&f.first), 0);
	assert_int_equal(bt_work_queue_deinit(&f.second), 0);
	assert_int_equal(osi_sem_deinit(&f.entered), 0);
	assert_int_equal(osi_sem_deinit(&f.release), 0);
	assert_int_equal(osi_sem_deinit(&f.done), 0);
	return 0;
}
static void block_worker(void)
{
	assert_int_equal(bt_work_submit_to_queue(&f.first, &f.gate), 1);
	assert_int_equal(osi_sem_take(&f.entered, 1000), 0);
}
static void wait_queued(struct bt_work *work)
{
	uint64_t now, end;
	osi_time_get_ms(&now);
	end = now + 1000;
	while (!(bt_work_busy_get(work) & BT_WORK_QUEUED)) {
		osi_time_get_ms(&now);
		assert_true(now < end);
		osi_sleep_ms(1);
	}
}
static void test_timeout_independent(void **state)
{
	(void)state;
	block_worker();
	assert_int_equal(bt_work_schedule_for_queue(&f.first, &f.delayed, BT_MSEC(20)), 1);
	wait_queued(&f.delayed.work);
	assert_false(bt_work_busy_get(&f.delayed.work) & BT_WORK_DELAYED);
	assert_int_equal(bt_work_submit_to_queue(&f.first, &f.immediate), 1);
	osi_sem_give(&f.release);
	assert_int_equal(osi_sem_take(&f.done, 1000), 0);
	assert_int_equal(osi_sem_take(&f.done, 1000), 0);
	assert_int_equal(f.order[0], 1);
	assert_int_equal(f.order[1], 2);
}
static void test_cross_queue_cancel(void **state)
{
	(void)state;
	block_worker();
	assert_int_equal(bt_work_submit_to_queue(&f.first, &f.delayed.work), 1);
	assert_int_equal(bt_work_reschedule_for_queue(&f.second, &f.delayed, BT_SECONDS(60)), 1);
	assert_ptr_equal(f.delayed.work.queue, &f.first);
	assert_ptr_equal(f.delayed.queue, &f.second);
	assert_int_equal(bt_work_cancel_delayable(&f.delayed), 0);
	assert_int_equal(bt_work_submit_to_queue(&f.first, &f.barrier), 1);
	osi_sem_give(&f.release);
	assert_int_equal(osi_sem_take(&f.done, 1000), 0);
	assert_int_equal(f.hits, 0);
	assert_int_equal(bt_work_busy_get(&f.delayed.work), 0);
}
static void test_schedule_and_forever(void **state)
{
	(void)state;
	block_worker();
	assert_int_equal(bt_work_schedule_for_queue(&f.first, &f.delayed, BT_FOREVER), 1);
	assert_true(bt_work_busy_get(&f.delayed.work) & BT_WORK_DELAYED);
	assert_int_equal(bt_work_schedule_for_queue(&f.first, &f.delayed, BT_NO_WAIT), 0);
	assert_int_equal(bt_work_reschedule_for_queue(&f.first, &f.delayed, BT_NO_WAIT), 1);
	assert_true(bt_work_busy_get(&f.delayed.work) & BT_WORK_QUEUED);
	assert_int_equal(bt_work_cancel_delayable(&f.delayed), 0);
	osi_sem_give(&f.release);
}
static void test_flush_delayable(void **state)
{
	struct bt_work_sync sync;
	(void)state;
	assert_int_equal(bt_work_schedule_for_queue(&f.first, &f.delayed, BT_SECONDS(60)), 1);
	assert_true(bt_work_flush_delayable(&f.delayed, &sync));
	assert_int_equal(f.hits, 1);
	assert_int_equal(bt_work_busy_get(&f.delayed.work), 0);
	assert_false(bt_work_flush_delayable(&f.delayed, &sync));
}
static void test_plug_and_stop(void **state)
{
	(void)state;
	assert_int_equal(bt_work_queue_stop(&f.first), -EBUSY);
	assert_int_equal(bt_work_queue_drain(&f.first, true), 0);
	assert_int_equal(bt_work_submit_to_queue(&f.first, &f.immediate), -EBUSY);
	assert_int_equal(bt_work_queue_unplug(&f.first), 0);
	assert_int_equal(bt_work_queue_unplug(&f.first), -EALREADY);
	assert_int_equal(bt_work_submit_to_queue(&f.first, &f.immediate), 1);
	assert_int_equal(osi_sem_take(&f.done, 1000), 0);
}

struct waiter {
	os_thread_t thread;
	struct bt_work_sync sync;
	os_sem_t done;
	bool cancel;
	bool drain;
	bool waited;
};
static void waiting_call(void *arg)
{
	struct waiter *w = arg;
	if (w->drain) {
		w->waited = bt_work_queue_drain(&f.first, false) == 0;
	} else if (w->cancel) {
		w->waited = bt_work_cancel_sync(&f.gate, &w->sync);
	} else {
		w->waited = bt_work_flush(&f.gate, &w->sync);
	}
	osi_sem_give(&w->done);
}
static void concurrent_waiters(bool drain)
{
	struct waiter waiters[3] = {0};
	block_worker();
	for (int i = 0; i < 3; i++) {
		waiters[i].drain = drain;
		waiters[i].cancel = i != 0;
		assert_int_equal(osi_sem_init(&waiters[i].done, 0, 1), 0);
		assert_int_equal(osi_thread_create(&waiters[i].thread, waiting_call, &waiters[i], NULL, 0, 0), 0);
	}
	osi_sleep_ms(20);
	for (int i = 0; i < 3; i++) {
		assert_int_equal(osi_sem_take(&waiters[i].done, 0), -EAGAIN);
	}
	osi_sem_give(&f.release);
	for (int i = 0; i < 3; i++) {
		assert_int_equal(osi_sem_take(&waiters[i].done, 1000), 0);
		assert_int_equal(osi_thread_join(&waiters[i].thread), 0);
		assert_true(waiters[i].waited);
		assert_int_equal(osi_sem_deinit(&waiters[i].done), 0);
	}
}
static void test_cancel_flush_waiters(void **state)
{
	(void)state;
	concurrent_waiters(false);
}
static void test_drain_waiters(void **state)
{
	(void)state;
	concurrent_waiters(true);
}
static void reused_timeout_handler(struct bt_work *work)
{
	(void)work;
}

static void test_timeout_cancel_reuse(void **state)
{
	struct bt_work_sync sync;
	(void)state;
	for (int i = 0; i < 100; i++) {
		struct bt_work_delayable *work = osi_malloc(sizeof(*work));
		assert_non_null(work);
		bt_work_init_delayable(work, reused_timeout_handler);
		assert_int_equal(bt_work_schedule_for_queue(&f.first, work, BT_MSEC(1)), 1);
		bt_work_cancel_delayable_sync(work, &sync);
		osi_free(work);
	}
	assert_int_equal(bt_work_queue_drain(&f.first, false), 0);
}
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_timeout_independent, setup, teardown),
		cmocka_unit_test_setup_teardown(test_cross_queue_cancel, setup, teardown),
		cmocka_unit_test_setup_teardown(test_schedule_and_forever, setup, teardown),
		cmocka_unit_test_setup_teardown(test_flush_delayable, setup, teardown),
		cmocka_unit_test_setup_teardown(test_plug_and_stop, setup, teardown),
		cmocka_unit_test_setup_teardown(test_cancel_flush_waiters, setup, teardown),
		cmocka_unit_test_setup_teardown(test_drain_waiters, setup, teardown),
		cmocka_unit_test_setup_teardown(test_timeout_cancel_reuse, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
