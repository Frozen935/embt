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

/** @file @brief LIFO ordering on the intrusive queue mechanism. */

#include <embt/base/lifo.h>
#include <embt/base/queue.h>

void bt_lifo_init(struct bt_lifo *q)
{
	bt_queue_init(&q->_queue);
}

int bt_lifo_deinit(struct bt_lifo *q)
{
	return bt_queue_deinit(&q->_queue);
}

void bt_lifo_put(struct bt_lifo *q, void *p)
{
	bt_queue_prepend(&q->_queue, p);
}

void *bt_lifo_get(struct bt_lifo *q, bt_timeout_t t)
{
	return bt_queue_get(&q->_queue, t);
}
