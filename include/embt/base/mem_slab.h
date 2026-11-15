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
 * @brief Fixed-size block allocation from bounded static storage.
 */

#ifndef EMBT_BASE_MEM_SLAB_H_
#define EMBT_BASE_MEM_SLAB_H_

#include <stddef.h>
#include <embt/compiler.h>
#include <embt/base/queue.h>
#include <embt/base/time.h>

/** @brief Fixed storage pool; blocks contain an intrusive free link while unused. */
struct bt_mem_slab {
	struct bt_queue free;
	void *storage;
	size_t block_size;
	size_t count;
};
#define BT_MEM_SLAB_DEFINE_STATIC(name, size, count_, align_)                                       \
	static unsigned char name##_storage[(size) * (count_)] __aligned(align_);                  \
	static struct bt_mem_slab name = {                                                          \
		.storage = name##_storage, .block_size = (size), .count = (count_)}
#define BT_MEM_SLAB_DEFINE_STATIC_TYPE(name, type, count)                                           \
	BT_MEM_SLAB_DEFINE_STATIC(name, sizeof(type), count, __alignof__(type))
/**
 * @brief Initialize all blocks of a fixed static pool.
 *
 * Requires exclusive ownership; no blocks may still be allocated.
 *
 * @param[in,out] slab Fixed block pool.
 */
void bt_mem_slab_init(struct bt_mem_slab *slab);
/**
 * @brief Allocate a fixed-size block from a static pool.
 *
 * The output is NULL on failure. Release successful allocations to the same pool.
 *
 * @param[in,out] slab Fixed block pool.
 * @param[out] out Receives the allocation or NULL.
 * @param[in] timeout Relative milliseconds; BT_NO_WAIT or BT_FOREVER when supported.
 * @return Zero on success, -ENOMEM on a nonblocking miss, or -EAGAIN after a wait fails.
 */
int bt_mem_slab_alloc(struct bt_mem_slab *slab, void **out, bt_timeout_t timeout);
/**
 * @brief Return a block to its owning static pool.
 *
 * The block must have been allocated from this pool and must not have been freed.
 *
 * @param[in,out] slab Fixed block pool.
 * @param[in] block Allocation to return.
 */
void bt_mem_slab_free(struct bt_mem_slab *slab, void *block);

/**
 * @brief Read the number of currently available blocks under the pool lock.
 * @param[in,out] slab Initialized fixed block pool.
 * @return Snapshot of available blocks; this does not reserve an allocation.
 */
size_t bt_mem_slab_num_free_get(struct bt_mem_slab *slab);

#endif /* EMBT_BASE_MEM_SLAB_H_ */
