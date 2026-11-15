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
 * @brief Serialized work queues with cancellable monotonic deadlines and explicit lifecycle.
 */

#include <base/base.h>

struct bt_work_runtime {
	struct k_work_q *owner;
	os_thread_t thread;
	os_cond_t changed;
	sys_slist_t pending;
	sys_slist_t timers;
	struct k_work *running;
	uint64_t running_seq;
	bool stopping;
	bool running_cancelled;
};

struct k_work_q k_sys_work_q;
static os_mutex_t work_lock;
static bool lock_initialized;

static void lock(void)
{
	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
}

static void unlock(void)
{
	bt_base_require(osi_mutex_unlock(&work_lock));
}

static bool running(const struct k_work *work)
{
	return work->queue && work->queue->runtime && work->queue->runtime->running == work;
}

static void changed(struct bt_work_runtime *rt)
{
	bt_base_require(osi_cond_broadcast(&rt->changed));
}

static int submit_locked(struct k_work_q *queue, struct k_work *work)
{
	struct bt_work_runtime *rt;
	bool active = running(work);

	if ((work->flags & K_WORK_CANCELING) ||
	    (active && work->queue->runtime->running_cancelled)) {
		return -EBUSY;
	}
	if (work->flags & K_WORK_QUEUED) {
		return 0;
	}
	if (active) {
		queue = work->queue;
	}
	rt = queue ? queue->runtime : NULL;
	if (!rt || rt->stopping) {
		return -ESHUTDOWN;
	}
	work->queue = queue;
	work->flags |= K_WORK_QUEUED;
	work->submitted++;
	sys_slist_append(&rt->pending, &work->node);
	changed(rt);
	return active ? 2 : 1;
}

static void remove_timer(struct k_work_delayable *work)
{
	if (work->work.flags & K_WORK_DELAYED) {
		struct bt_work_runtime *rt = work->timer_queue->runtime;

		sys_slist_find_and_remove(&rt->timers, &work->timer_node);
		work->work.flags &= ~K_WORK_DELAYED;
		work->timer_queue = NULL;
		changed(rt);
	}
}

static void worker(void *arg)
{
	struct bt_work_runtime *rt = arg;

	lock();
	while (!rt->stopping) {
		struct k_work_delayable *timer;
		struct k_work *work;
		sys_snode_t *node;
		os_timeout_t wait = OS_TIMEOUT_FOREVER;

		while ((node = sys_slist_peek_head(&rt->timers)) != NULL) {
			timer = CONTAINER_OF(node, struct k_work_delayable, timer_node);
			wait = timer->deadline - k_uptime_get();
			if (wait > 0) {
				break;
			}
			remove_timer(timer);
			submit_locked(rt->owner, &timer->work);
			wait = OS_TIMEOUT_FOREVER;
		}
		node = sys_slist_get(&rt->pending);
		if (!node) {
			int err = osi_cond_wait(&rt->changed, &work_lock, wait);

			if (err != -ETIMEDOUT) {
				bt_base_require(err);
			}
			continue;
		}
		work = CONTAINER_OF(node, struct k_work, node);
		work->flags &= ~K_WORK_QUEUED;
		rt->running = work;
		rt->running_seq = work->submitted;
		rt->running_cancelled = false;
		unlock();
		work->handler(work);
		lock();
		/* A handler may release its own container if it did not resubmit it. */
		rt->running = NULL;
		changed(rt);
	}
	unlock();
}

void k_work_init(struct k_work *work, void (*handler)(struct k_work *))
{
	*work = (struct k_work){.handler = handler};
}

void k_work_init_delayable(struct k_work_delayable *work, void (*handler)(struct k_work *))
{
	*work = (struct k_work_delayable){.work = {.handler = handler}};
}

void k_work_queue_init(struct k_work_q *queue)
{
	queue->runtime = NULL;
}

int bt_work_queue_start(struct k_work_q *queue, const char *name, size_t stack_size)
{
	struct bt_work_runtime *rt;
	int err;

	if (queue->runtime) {
		return -EALREADY;
	}
	/* Queue lifecycle operations are serialized by the stack owner. */
	if (!lock_initialized) {
		err = osi_mutex_init(&work_lock, OS_MUTEX_NORMAL);
		if (err) {
			return err;
		}
		lock_initialized = true;
	}
	rt = osi_calloc(1, sizeof(*rt));
	if (!rt) {
		return -ENOMEM;
	}
	err = osi_cond_init(&rt->changed);
	if (err) {
		osi_free(rt);
		return err;
	}
	rt->owner = queue;
	queue->runtime = rt;
	err = osi_thread_create(&rt->thread, worker, rt, name, 0, stack_size);
	if (err) {
		queue->runtime = NULL;
		osi_cond_deinit(&rt->changed);
		osi_free(rt);
	}
	return err;
}

int bt_work_queue_stop(struct k_work_q *queue)
{
	struct bt_work_runtime *rt = queue->runtime;
	sys_snode_t *node;
	int err;

	if (!rt) {
		return 0;
	}
	if (osi_thread_is_current(&rt->thread)) {
		return -EDEADLK;
	}
	lock();
	rt->stopping = true;
	while ((node = sys_slist_get(&rt->pending)) != NULL) {
		struct k_work *work = CONTAINER_OF(node, struct k_work, node);

		work->flags &= ~K_WORK_QUEUED;
	}
	while ((node = sys_slist_peek_head(&rt->timers)) != NULL) {
		remove_timer(CONTAINER_OF(node, struct k_work_delayable, timer_node));
	}
	changed(rt);
	unlock();
	err = osi_thread_join(&rt->thread);
	if (err) {
		return err;
	}
	queue->runtime = NULL;
	bt_base_require(osi_cond_deinit(&rt->changed));
	osi_free(rt);
	return 0;
}

k_tid_t k_work_queue_thread_get(struct k_work_q *queue)
{
	return queue->runtime->thread.native;
}

int k_work_submit_to_queue(struct k_work_q *queue, struct k_work *work)
{
	int ret;

	lock();
	ret = submit_locked(queue, work);
	unlock();
	return ret;
}

static int schedule(struct k_work_q *queue, struct k_work_delayable *work, k_timeout_t timeout,
		    bool replace)
{
	struct bt_work_runtime *rt;
	sys_snode_t *prev = NULL;
	sys_snode_t *node;
	int ret = 1;

	if (timeout.ticks < -1 || timeout.ticks == -1) {
		return -EINVAL;
	}
	lock();
	if (!queue || !queue->runtime || queue->runtime->stopping) {
		ret = -ESHUTDOWN;
		goto out;
	}
	if ((work->work.flags & K_WORK_CANCELING) ||
	    (running(&work->work) && work->work.queue->runtime->running_cancelled)) {
		ret = -EBUSY;
		goto out;
	}
	if (!replace && (work->work.flags & (K_WORK_DELAYED | K_WORK_QUEUED))) {
		ret = 0;
		goto out;
	}
	remove_timer(work);
	if (timeout.ticks == 0) {
		ret = submit_locked(queue, &work->work);
		goto out;
	}
	rt = queue->runtime;
	work->deadline = sys_timepoint_calc(timeout).ticks;
	work->timer_queue = queue;
	if (!running(&work->work)) {
		work->work.queue = queue;
	}
	work->work.flags |= K_WORK_DELAYED;
	SYS_SLIST_FOR_EACH_NODE(&rt->timers, node) {
		struct k_work_delayable *entry =
			CONTAINER_OF(node, struct k_work_delayable, timer_node);

		if (entry->deadline > work->deadline) {
			break;
		}
		prev = node;
	}
	sys_slist_insert(&rt->timers, prev, &work->timer_node);
	changed(rt);
out:
	unlock();
	return ret;
}

int k_work_schedule_for_queue(struct k_work_q *q, struct k_work_delayable *w, k_timeout_t t)
{
	return schedule(q, w, t, false);
}

int k_work_reschedule_for_queue(struct k_work_q *q, struct k_work_delayable *w, k_timeout_t t)
{
	return schedule(q, w, t, true);
}

static int cancel_locked(struct k_work *work)
{
	if (work->flags & K_WORK_QUEUED) {
		struct bt_work_runtime *rt = work->queue->runtime;

		sys_slist_find_and_remove(&rt->pending, &work->node);
		work->flags &= ~K_WORK_QUEUED;
		changed(rt);
	}
	if (running(work)) {
		work->queue->runtime->running_cancelled = true;
		return K_WORK_RUNNING | K_WORK_CANCELING;
	}
	return 0;
}

int k_work_cancel(struct k_work *work)
{
	int ret;

	lock();
	ret = cancel_locked(work);
	unlock();
	return ret;
}

int k_work_cancel_delayable(struct k_work_delayable *work)
{
	int ret;

	lock();
	remove_timer(work);
	ret = cancel_locked(&work->work);
	unlock();
	return ret;
}

static bool cancel_sync(struct k_work *work, struct k_work_delayable *delayable)
{
	bool busy;

	lock();
	busy = work->flags || running(work);
	if (delayable) {
		remove_timer(delayable);
	}
	cancel_locked(work);
	work->flags |= K_WORK_CANCELING;
	while (running(work)) {
		struct bt_work_runtime *rt = work->queue->runtime;

		__ASSERT_NO_MSG(!osi_thread_is_current(&rt->thread));
		bt_base_require(osi_cond_wait(&rt->changed, &work_lock, OS_TIMEOUT_FOREVER));
	}
	work->flags &= ~K_WORK_CANCELING;
	unlock();
	return busy;
}

bool k_work_cancel_sync(struct k_work *work, struct k_work_sync *sync)
{
	(void)sync;
	return cancel_sync(work, NULL);
}

bool k_work_cancel_delayable_sync(struct k_work_delayable *work, struct k_work_sync *sync)
{
	(void)sync;
	return cancel_sync(&work->work, work);
}

bool k_work_flush(struct k_work *work, struct k_work_sync *sync)
{
	uint64_t goal;
	bool busy;

	(void)sync;
	lock();
	goal = work->submitted;
	busy = (work->flags & K_WORK_QUEUED) || running(work);
	for (;;) {
		struct bt_work_runtime *rt = work->queue ? work->queue->runtime : NULL;
		bool pending = (work->flags & K_WORK_QUEUED) && work->submitted <= goal;
		bool active = running(work) && rt->running_seq <= goal;

		if (!pending && !active) {
			break;
		}
		__ASSERT_NO_MSG(!osi_thread_is_current(&rt->thread));
		bt_base_require(osi_cond_wait(&rt->changed, &work_lock, OS_TIMEOUT_FOREVER));
	}
	unlock();
	return busy;
}

unsigned int k_work_busy_get(const struct k_work *work)
{
	unsigned int flags;

	lock();
	flags = work->flags;
	if (running(work)) {
		flags |= K_WORK_RUNNING;
		if (work->queue->runtime->running_cancelled) {
			flags |= K_WORK_CANCELING;
		}
	}
	unlock();
	return flags;
}

int64_t k_work_delayable_remaining_get(const struct k_work_delayable *work)
{
	int64_t remaining;

	lock();
	remaining = work->work.flags & K_WORK_DELAYED
			    ? MAX(INT64_C(0), work->deadline - k_uptime_get())
			    : 0;
	unlock();
	return remaining;
}
