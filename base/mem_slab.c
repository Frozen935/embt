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

/** @file @brief Bounded aligned block pools backed by intrusive queues. */

#include <embt/base/mem_slab.h>
#include <embt/base/queue.h>
#include <embt/base/assert.h>
#include <embt/base/base.h>
#include <embt/osi/sync.h>
#include <errno.h>

void bt_mem_slab_init(struct bt_mem_slab *slab)
{
	__ASSERT_NO_MSG(slab->block_size >= sizeof(sys_snode_t));
	__ASSERT_NO_MSG(slab->block_size % __alignof__(sys_snode_t) == 0);
	bt_queue_init(&slab->free);
	for (size_t i = 0; i < slab->count; i++) {
		bt_queue_append(&slab->free, (char *)slab->storage + i * slab->block_size);
	}
}

int bt_mem_slab_alloc(struct bt_mem_slab *slab, void **out, bt_timeout_t timeout)
{
	*out = bt_queue_get(&slab->free, timeout);
	return *out ? 0 : (timeout == 0 ? -ENOMEM : -EAGAIN);
}

void bt_mem_slab_free(struct bt_mem_slab *slab, void *block)
{
	uintptr_t offset = (uintptr_t)block - (uintptr_t)slab->storage;

	__ASSERT_NO_MSG(offset < slab->block_size * slab->count);
	__ASSERT_NO_MSG(offset % slab->block_size == 0);
	bt_queue_prepend(&slab->free, block);
}

size_t bt_mem_slab_num_free_get(struct bt_mem_slab *slab)
{
	size_t count = 0;
	sys_snode_t *node;

	bt_base_require(osi_mutex_lock(&slab->free.mutex, OS_TIMEOUT_FOREVER));
	SYS_SLIST_FOR_EACH_NODE(&slab->free.data, node) {
		count++;
	}
	bt_base_require(osi_mutex_unlock(&slab->free.mutex));
	return count;
}
