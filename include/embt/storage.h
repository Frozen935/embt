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
 * @brief Protocol object storage without OSI or atomic API exposure.
 */

#ifndef EMBT_STORAGE_H_
#define EMBT_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <embt/sys/slist.h>
#include <embt/sys/util.h>

/** @brief Relative milliseconds; -1 means forever and zero means no wait. */
typedef struct {
	int64_t ticks;
} k_timeout_t;
/** @brief Absolute monotonic milliseconds used by internal wait operations. */
typedef struct {
	int64_t ticks;
} k_timepoint_t;
#define K_MSEC(ms)         ((k_timeout_t){.ticks = (ms)})
#define K_SECONDS(s)       K_MSEC(INT64_C(1000) * (s))
#define K_MINUTES(m)       K_SECONDS(INT64_C(60) * (m))
#define K_NO_WAIT          K_MSEC(0)
#define K_FOREVER          K_MSEC(-1)
#define K_TIMEOUT_EQ(a, b) ((a).ticks == (b).ticks)

/**
 * @brief Internal intrusive queue storage embedded in public protocol objects.
 *
 * Contains no native OS objects or public synchronization API. Initialize through
 * the owning protocol API. Never access these fields concurrently from an application.
 */
struct k_queue {
	sys_slist_t data;
	sys_slist_t waiters;
};
struct k_fifo {
	struct k_queue _queue;
};
struct k_lifo {
	struct k_queue _queue;
};
/** @brief Internal lock storage; only the stack may operate on it. */
struct k_spinlock {
	unsigned char locked;
};
/** @brief Internal bounded semaphore storage embedded in protocol objects. */
struct k_sem {
	unsigned int count;
	unsigned int limit;
	sys_slist_t waiters;
};
struct k_work_q;
struct k_work;
/** @brief Internal deferred handler storage, without native timer resources. */
struct k_work {
	sys_snode_t node;
	void (*handler)(struct k_work *work);
	struct k_work_q *queue;
	unsigned int flags;
	uint64_t submitted;
};
/** @brief Internal deadline and ordinary work storage. */
struct k_work_delayable {
	struct k_work work;
	sys_snode_t timer_node;
	int64_t deadline;
	struct k_work_q *timer_queue;
};
/** @brief Caller-owned synchronization token for synchronous work completion. */
struct k_work_sync {
	unsigned int reserved;
};
/** @brief Bounded arena metadata, operated on only by internal buffer allocators. */
struct k_heap {
	unsigned char *storage;
	size_t size;
	bool initialized;
};
#define K_HEAP_DEFINE(name, bytes)                                                                 \
	static unsigned char name##_storage[ROUND_UP(bytes, sizeof(void *))]                       \
		__aligned(sizeof(void *));                                                         \
	static struct k_heap name = {.storage = name##_storage, .size = sizeof(name##_storage)}

#endif /* EMBT_STORAGE_H_ */
