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

/** @file @brief Intrusive FIFO queues with bounded waits and cancellation. */

#include <embt/base/queue.h>
#include <embt/base/time.h>
#include <embt/base/assert.h>
#include <embt/base/util.h>
#include <embt/base/base.h>
#include <embt/osi/sync.h>

struct queue_waiter {
	sys_snode_t node;
	os_sem_t wake;
	void *item;
	bool completed;
};

void bt_queue_init(struct bt_queue *queue)
{
	/* The owner serializes initialization and excludes all current users. */
	if (!queue->initialized) {
		bt_base_require(osi_mutex_init(&queue->mutex, OS_MUTEX_NORMAL));
		queue->active_waiters = 0;
		queue->initialized = true;
	}
	bt_base_require(queue->active_waiters ? -EBUSY : 0);
	sys_slist_init(&queue->data);
	sys_slist_init(&queue->waiters);
}

int bt_queue_deinit(struct bt_queue *queue)
{
	int err;

	if (!queue->initialized) {
		return 0;
	}
	/* Lifecycle ownership excludes new users, including new waiting gets. */
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	err = queue->active_waiters ? -EBUSY : 0;
	bt_base_require(osi_mutex_unlock(&queue->mutex));
	if (err) {
		return err;
	}
	err = osi_mutex_deinit(&queue->mutex);
	if (err) {
		return err;
	}
	queue->initialized = false;
	sys_slist_init(&queue->data);
	sys_slist_init(&queue->waiters);
	return 0;
}

static void queue_put(struct bt_queue *queue, void *item, bool prepend)
{
	struct queue_waiter *waiter;
	sys_snode_t *node;

	__ASSERT_NO_MSG(item != NULL);
	bt_base_require(queue->initialized ? 0 : -EINVAL);
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	node = sys_slist_get(&queue->waiters);
	if (node) {
		waiter = CONTAINER_OF(node, struct queue_waiter, node);
		waiter->item = item;
		waiter->completed = true;
		/* Signal while holding the lock: the waiter cannot destroy wake yet. */
		bt_base_require(osi_sem_give(&waiter->wake));
	} else if (prepend) {
		sys_slist_prepend(&queue->data, item);
	} else {
		sys_slist_append(&queue->data, item);
	}
	bt_base_require(osi_mutex_unlock(&queue->mutex));
}

void bt_queue_append(struct bt_queue *queue, void *item)
{
	queue_put(queue, item, false);
}

void bt_queue_prepend(struct bt_queue *queue, void *item)
{
	queue_put(queue, item, true);
}

void *bt_queue_get(struct bt_queue *queue, bt_timeout_t timeout)
{
	struct queue_waiter waiter = {0};
	bt_timepoint_t end = bt_timepoint_calc(timeout);
	void *item;
	int err;

	if (timeout < -1) {
		return NULL;
	}
	bt_base_require(queue->initialized ? 0 : -EINVAL);
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	item = sys_slist_get(&queue->data);
	if (item || timeout == 0) {
		bt_base_require(osi_mutex_unlock(&queue->mutex));
		return item;
	}
	err = osi_sem_init(&waiter.wake, 0, 1);
	if (err) {
		bt_base_require(osi_mutex_unlock(&queue->mutex));
		return NULL;
	}
	sys_slist_append(&queue->waiters, &waiter.node);
	queue->active_waiters++;
	bt_base_require(osi_mutex_unlock(&queue->mutex));
	err = osi_sem_take(&waiter.wake, bt_timepoint_timeout(end));
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	if (!waiter.completed) {
		sys_slist_find_and_remove(&queue->waiters, &waiter.node);
	}
	queue->active_waiters--;
	item = waiter.item;
	bt_base_require(osi_mutex_unlock(&queue->mutex));
	bt_base_require(osi_sem_deinit(&waiter.wake));
	if (err && err != -ETIMEDOUT && err != -EAGAIN) {
		bt_base_require(err);
	}
	return item;
}

void bt_queue_cancel_wait(struct bt_queue *queue)
{
	sys_snode_t *node;

	bt_base_require(queue->initialized ? 0 : -EINVAL);
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	node = sys_slist_get(&queue->waiters);
	if (node) {
		struct queue_waiter *waiter = CONTAINER_OF(node, struct queue_waiter, node);

		waiter->completed = true;
		bt_base_require(osi_sem_give(&waiter->wake));
	}
	bt_base_require(osi_mutex_unlock(&queue->mutex));
}

bool bt_queue_remove(struct bt_queue *queue, void *item)
{
	bool removed;

	bt_base_require(queue->initialized ? 0 : -EINVAL);
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	removed = sys_slist_find_and_remove(&queue->data, item);
	bt_base_require(osi_mutex_unlock(&queue->mutex));
	return removed;
}

void *bt_queue_peek_head(struct bt_queue *queue)
{
	void *item;

	bt_base_require(queue->initialized ? 0 : -EINVAL);
	bt_base_require(osi_mutex_lock(&queue->mutex, OS_TIMEOUT_FOREVER));
	item = sys_slist_peek_head(&queue->data);
	bt_base_require(osi_mutex_unlock(&queue->mutex));
	return item;
}
