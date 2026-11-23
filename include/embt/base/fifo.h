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
 * @brief FIFO operations on caller-owned intrusive storage.
 */

#ifndef EMBT_BASE_FIFO_H_
#define EMBT_BASE_FIFO_H_

#include <stdbool.h>
#include <embt/base/queue.h>
#include <embt/base/time.h>

struct bt_fifo {
	struct bt_queue _queue;
};


/**
 * @brief Initialize zeroed or quiescent FIFO storage with no active users or waiters.
 * @param[out] queue Caller-owned queue storage.
 */
void bt_fifo_init(struct bt_fifo *queue);
/**
 * @brief Release native queue resources before freeing or overwriting storage.
 * @param[in,out] queue Queue whose owner excludes concurrent and new users.
 * @return Zero on success, -EBUSY for an outstanding waiter, or a native error.
 * @note Borrowed items are detached without being freed.
 */
int bt_fifo_deinit(struct bt_fifo *queue);
/**
 * @brief Transfer an intrusive item to a waiter or the FIFO.
 * @param[in,out] queue Initialized queue.
 * @param[in,out] item Item beginning with a sys_snode_t, not currently linked.
 */
void bt_fifo_put(struct bt_fifo *queue, void *item);
/**
 * @brief Take the next item using one monotonic timeout budget.
 * @param[in,out] queue Initialized queue.
 * @param[in] timeout Relative milliseconds, BT_NO_WAIT or BT_FOREVER.
 * @return An owned item, or NULL if unavailable, expired or cancelled.
 */
void *bt_fifo_get(struct bt_fifo *queue, bt_timeout_t timeout);
/**
 * @brief Borrow the first item without removing it.
 * @param[in,out] queue Initialized FIFO; caller protects the item's lifetime.
 * @return Borrowed head item, or NULL if empty.
 */
void *bt_fifo_peek_head(struct bt_fifo *queue);
/**
 * @brief Read a snapshot of whether a FIFO has queued items.
 * @param[in,out] queue Initialized FIFO.
 * @return True if empty; the result does not reserve an item.
 */
bool bt_fifo_is_empty(struct bt_fifo *queue);
/**
 * @brief Wake the oldest waiter with NULL; later waits are unaffected.
 * @param[in,out] queue Initialized FIFO.
 */
void bt_fifo_cancel_wait(struct bt_fifo *queue);
#define BT_FIFO_DEFINE(name) struct bt_fifo name

#endif /* EMBT_BASE_FIFO_H_ */
