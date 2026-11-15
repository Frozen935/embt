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

/** @file @brief FIFO ordering on the intrusive queue mechanism. */

#include <embt/base/fifo.h>
#include <embt/base/queue.h>

void bt_fifo_init(struct bt_fifo *q)
{
	bt_queue_init(&q->_queue);
}

int bt_fifo_deinit(struct bt_fifo *q)
{
	return bt_queue_deinit(&q->_queue);
}

void bt_fifo_put(struct bt_fifo *q, void *p)
{
	bt_queue_append(&q->_queue, p);
}

void *bt_fifo_get(struct bt_fifo *q, bt_timeout_t t)
{
	return bt_queue_get(&q->_queue, t);
}

void *bt_fifo_peek_head(struct bt_fifo *q)
{
	return bt_queue_peek_head(&q->_queue);
}

bool bt_fifo_is_empty(struct bt_fifo *q)
{
	return bt_fifo_peek_head(q) == NULL;
}

void bt_fifo_cancel_wait(struct bt_fifo *q)
{
	bt_queue_cancel_wait(&q->_queue);
}
