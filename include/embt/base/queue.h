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
 * @brief Intrusive queues with direct handoff and cancellable waits.
 */

#ifndef EMBT_BASE_QUEUE_H_
#define EMBT_BASE_QUEUE_H_

#include <stdbool.h>
#include <stddef.h>
#include <embt/base/slist.h>
#include <embt/osi/sync.h>
#include <embt/base/time.h>

/** @brief Internal queue storage; initialize explicitly and never copy an active queue. */
struct bt_queue {
	sys_slist_t data;
	sys_slist_t waiters;
	os_mutex_t mutex;
	size_t active_waiters;
	bool initialized;
};


/**
 * @brief Initialize an empty intrusive queue.
 *
 * Zero storage before the first init. Call init before every first use; queue
 * operations never create resources. A quiescent reinitialization clears item
 * links and retains the mutex. The owner handles old items and serializes init.
 *
 * @param[in,out] queue Queue owned by the caller.
 */
void bt_queue_init(struct bt_queue *queue);
/**
 * @brief Release the queue mutex before freeing or overwriting its storage.
 * @param[in,out] queue Queue with no concurrent operations or new producers.
 * @return Zero on success, -EBUSY while a waiting get is outstanding, or a
 * negative native error. On success the queue can be initialized again.
 * @note Borrowed queued items are detached, not freed. The owner handles their
 * lifetime and joins all users before releasing the queue itself.
 */
int bt_queue_deinit(struct bt_queue *queue);
/**
 * @brief Append an item or transfer it to the oldest waiter.
 *
 * The item starts with a sys_snode_t and must not already be linked in a queue.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] item Intrusive item whose ownership is transferred or queried.
 */
void bt_queue_append(struct bt_queue *queue, void *item);
/**
 * @brief Prepend an item or transfer it to the oldest waiter.
 *
 * The item starts with a sys_snode_t and must not already be linked in a queue.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] item Intrusive item whose ownership is transferred or queried.
 */
void bt_queue_prepend(struct bt_queue *queue, void *item);
/**
 * @brief Remove a queued item by its address.
 *
 * This cannot revoke ownership already transferred to a consumer.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] item Intrusive item whose ownership is transferred or queried.
 * @return True if the condition holds, otherwise false.
 */
bool bt_queue_remove(struct bt_queue *queue, void *item);
/** @brief Borrow the head under the queue lock; caller must protect item lifetime. */
void *bt_queue_peek_head(struct bt_queue *queue);
/** @brief Take an item with one monotonic deadline; NULL means empty, cancelled or expired. */
void *bt_queue_get(struct bt_queue *queue, bt_timeout_t timeout);
/**
 * @brief Cancel the oldest current waiter.
 *
 * Future waits are unaffected; a successful cancellation returns NULL to that waiter.
 *
 * @param[in,out] queue Queue owned by the caller.
 */
void bt_queue_cancel_wait(struct bt_queue *queue);

#endif /* EMBT_BASE_QUEUE_H_ */
