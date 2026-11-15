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
 * @brief Queue, fixed-block pool, time and lock mechanisms built on OSI.
 */

#include <base/base.h>

static os_mutex_t queue_lock;
static os_cond_t heap_changed;
static os_mutex_t scheduler_lock;
static bool initialized;

void bt_base_require(int result)
{
	if (result != 0) {
		osi_fatal("EmBT base operation failed");
	}
}

int bt_base_init(void)
{
	int err;

	/* The stack lifecycle entry serializes initialization and shutdown. */
	if (initialized) {
		return 0;
	}
	err = osi_mutex_init(&queue_lock, OS_MUTEX_NORMAL);
	if (err) {
		return err;
	}
	err = osi_mutex_init(&scheduler_lock, OS_MUTEX_RECURSIVE);
	if (err) {
		osi_mutex_deinit(&queue_lock);
		return err;
	}
	err = osi_cond_init(&heap_changed);
	if (err) {
		osi_mutex_deinit(&scheduler_lock);
		osi_mutex_deinit(&queue_lock);
		return err;
	}
	err = bt_work_queue_start(&k_sys_work_q, "embt-system", 0);
	if (err) {
		osi_cond_deinit(&heap_changed);
		osi_mutex_deinit(&scheduler_lock);
		osi_mutex_deinit(&queue_lock);
		return err;
	}
	initialized = true;
	return 0;
}

int bt_base_deinit(void)
{
	int err;

	if (!initialized) {
		return 0;
	}
	err = bt_work_queue_stop(&k_sys_work_q);
	if (err) {
		return err;
	}
	bt_base_require(osi_mutex_deinit(&scheduler_lock));
	bt_base_require(osi_cond_deinit(&heap_changed));
	bt_base_require(osi_mutex_deinit(&queue_lock));
	initialized = false;
	return 0;
}

int64_t k_uptime_get(void)
{
	uint64_t ms;

	bt_base_require(osi_time_get_ms(&ms));
	return (int64_t)ms;
}

uint32_t k_uptime_get_32(void)
{
	return (uint32_t)k_uptime_get();
}

k_timepoint_t sys_timepoint_calc(k_timeout_t timeout)
{
	int64_t now = k_uptime_get();

	return (k_timepoint_t){.ticks = timeout.ticks < 0 ? INT64_MAX
							  : (timeout.ticks > INT64_MAX - now
								     ? INT64_MAX
								     : now + timeout.ticks)};
}

k_timeout_t sys_timepoint_timeout(k_timepoint_t end)
{
	if (end.ticks == INT64_MAX) {
		return K_FOREVER;
	}
	return K_MSEC(MAX(INT64_C(0), end.ticks - k_uptime_get()));
}

bool sys_timepoint_expired(k_timepoint_t end)
{
	return end.ticks != INT64_MAX && k_uptime_get() >= end.ticks;
}

k_spinlock_key_t k_spin_lock(struct k_spinlock *lock)
{
	while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
		osi_thread_yield();
	}
	return 0;
}

void k_spin_unlock(struct k_spinlock *lock, k_spinlock_key_t key)
{
	(void)key;
	__atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

void k_sched_lock(void)
{
	bt_base_require(osi_mutex_lock(&scheduler_lock, OS_TIMEOUT_FOREVER));
}

void k_sched_unlock(void)
{
	bt_base_require(osi_mutex_unlock(&scheduler_lock));
}

struct queue_waiter {
	sys_snode_t node;
	os_sem_t wake;
	void *item;
	bool completed;
};

static void queue_acquire(void)
{
	bt_base_require(osi_mutex_lock(&queue_lock, OS_TIMEOUT_FOREVER));
}

static void queue_release(void)
{
	bt_base_require(osi_mutex_unlock(&queue_lock));
}

void k_queue_init(struct k_queue *queue)
{
	sys_slist_init(&queue->data);
	sys_slist_init(&queue->waiters);
}

static void queue_put(struct k_queue *queue, void *item, bool prepend)
{
	struct queue_waiter *waiter;
	sys_snode_t *node;

	__ASSERT_NO_MSG(item != NULL);
	queue_acquire();
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
	queue_release();
}

void k_queue_append(struct k_queue *queue, void *item)
{
	queue_put(queue, item, false);
}

void k_queue_prepend(struct k_queue *queue, void *item)
{
	queue_put(queue, item, true);
}

void *k_queue_get(struct k_queue *queue, k_timeout_t timeout)
{
	struct queue_waiter waiter = {0};
	k_timepoint_t end = sys_timepoint_calc(timeout);
	void *item;
	int err;

	if (timeout.ticks < -1) {
		return NULL;
	}
	queue_acquire();
	item = sys_slist_get(&queue->data);
	if (item || timeout.ticks == 0) {
		queue_release();
		return item;
	}
	err = osi_sem_init(&waiter.wake, 0, 1);
	if (err) {
		queue_release();
		return NULL;
	}
	sys_slist_append(&queue->waiters, &waiter.node);
	queue_release();
	err = osi_sem_take(&waiter.wake, sys_timepoint_timeout(end).ticks);
	queue_acquire();
	if (!waiter.completed) {
		sys_slist_find_and_remove(&queue->waiters, &waiter.node);
	}
	item = waiter.item;
	queue_release();
	bt_base_require(osi_sem_deinit(&waiter.wake));
	if (err && err != -ETIMEDOUT && err != -EAGAIN) {
		bt_base_require(err);
	}
	return item;
}

void k_queue_cancel_wait(struct k_queue *queue)
{
	sys_snode_t *node;

	queue_acquire();
	node = sys_slist_get(&queue->waiters);
	if (node) {
		struct queue_waiter *waiter = CONTAINER_OF(node, struct queue_waiter, node);

		waiter->completed = true;
		bt_base_require(osi_sem_give(&waiter->wake));
	}
	queue_release();
}

bool k_queue_remove(struct k_queue *queue, void *item)
{
	bool removed;

	queue_acquire();
	removed = sys_slist_find_and_remove(&queue->data, item);
	queue_release();
	return removed;
}

void *k_queue_peek_head(struct k_queue *queue)
{
	void *item;

	queue_acquire();
	item = sys_slist_peek_head(&queue->data);
	queue_release();
	return item;
}

void bt_mem_slab_init(struct k_mem_slab *slab)
{
	__ASSERT_NO_MSG(slab->block_size >= sizeof(sys_snode_t));
	__ASSERT_NO_MSG(slab->block_size % __alignof__(sys_snode_t) == 0);
	k_queue_init(&slab->free);
	for (size_t i = 0; i < slab->count; i++) {
		k_queue_append(&slab->free, (char *)slab->storage + i * slab->block_size);
	}
}

int k_mem_slab_alloc(struct k_mem_slab *slab, void **out, k_timeout_t timeout)
{
	*out = k_queue_get(&slab->free, timeout);
	return *out ? 0 : (timeout.ticks == 0 ? -ENOMEM : -EAGAIN);
}

void k_mem_slab_free(struct k_mem_slab *slab, void *block)
{
	uintptr_t offset = (uintptr_t)block - (uintptr_t)slab->storage;

	__ASSERT_NO_MSG(offset < slab->block_size * slab->count);
	__ASSERT_NO_MSG(offset % slab->block_size == 0);
	k_queue_prepend(&slab->free, block);
}

struct heap_block {
	size_t size;
	bool used;
};

void *k_heap_aligned_alloc(struct k_heap *heap, size_t alignment, size_t size, k_timeout_t timeout)
{
	k_timepoint_t end = sys_timepoint_calc(timeout);
	void *result = NULL;

	if (!size || alignment < sizeof(void *) || (alignment & (alignment - 1)) ||
	    size > heap->size || timeout.ticks < -1) {
		return NULL;
	}
	queue_acquire();
	if (!heap->initialized) {
		struct heap_block *first = (void *)heap->storage;

		if (heap->size < sizeof(*first) + sizeof(void *)) {
			goto out;
		}
		*first = (struct heap_block){.size = heap->size};
		heap->initialized = true;
	}
	for (;;) {
		unsigned char *end_addr = heap->storage + heap->size;

		for (unsigned char *p = heap->storage; p < end_addr;) {
			struct heap_block *block = (void *)p;
			uintptr_t payload = ROUND_UP(
				(uintptr_t)(p + sizeof(*block) + sizeof(void *)), alignment);
			size_t prefix = payload - (uintptr_t)p;

			if (!block->used && prefix <= block->size && size <= block->size - prefix) {
				size_t used = ROUND_UP(prefix + size, sizeof(void *));

				if (used <= block->size &&
				    block->size - used >= sizeof(*block) + sizeof(void *)) {
					struct heap_block *next = (void *)(p + used);

					*next = (struct heap_block){.size = block->size - used};
					block->size = used;
				}
				block->used = true;
				((struct heap_block **)payload)[-1] = block;
				result = (void *)payload;
				goto out;
			}
			p += block->size;
		}
		if (timeout.ticks == 0 || sys_timepoint_expired(end)) {
			break;
		}
		int err =
			osi_cond_wait(&heap_changed, &queue_lock, sys_timepoint_timeout(end).ticks);

		if (err == -ETIMEDOUT) {
			break;
		}
		bt_base_require(err);
	}
out:
	queue_release();
	return result;
}

void k_heap_free(struct k_heap *heap, void *data)
{
	struct heap_block *released;
	struct heap_block *prev = NULL;
	unsigned char *end = heap->storage + heap->size;

	if (!data) {
		return;
	}
	queue_acquire();
	released = ((struct heap_block **)data)[-1];
	__ASSERT_NO_MSG((unsigned char *)released >= heap->storage &&
			(unsigned char *)released < end);
	__ASSERT_NO_MSG(released->used);
	released->used = false;
	for (unsigned char *p = heap->storage; p < end;) {
		struct heap_block *block = (void *)p;

		if (prev && !prev->used && !block->used) {
			prev->size += block->size;
		} else {
			prev = block;
		}
		p += block->size;
	}
	bt_base_require(osi_cond_broadcast(&heap_changed));
	queue_release();
}

int k_sem_init(struct k_sem *sem, unsigned int count, unsigned int limit)
{
	if (!sem || !limit || count > limit) {
		return -EINVAL;
	}
	*sem = (struct k_sem){.count = count, .limit = limit};
	return 0;
}

int k_sem_take(struct k_sem *sem, k_timeout_t timeout)
{
	struct queue_waiter waiter = {0};
	k_timepoint_t end = sys_timepoint_calc(timeout);
	int err;

	if (timeout.ticks < -1) {
		return -EINVAL;
	}
	queue_acquire();
	if (sem->count) {
		sem->count--;
		queue_release();
		return 0;
	}
	if (timeout.ticks == 0) {
		queue_release();
		return -EAGAIN;
	}
	err = osi_sem_init(&waiter.wake, 0, 1);
	if (err) {
		queue_release();
		return err;
	}
	sys_slist_append(&sem->waiters, &waiter.node);
	queue_release();
	err = osi_sem_take(&waiter.wake, sys_timepoint_timeout(end).ticks);
	queue_acquire();
	if (!waiter.completed) {
		sys_slist_find_and_remove(&sem->waiters, &waiter.node);
	}
	queue_release();
	bt_base_require(osi_sem_deinit(&waiter.wake));
	if (waiter.item) {
		return 0;
	}
	return err && err != -ETIMEDOUT ? err : -EAGAIN;
}

void k_sem_give(struct k_sem *sem)
{
	sys_snode_t *node;

	queue_acquire();
	node = sys_slist_get(&sem->waiters);
	if (node) {
		struct queue_waiter *waiter = CONTAINER_OF(node, struct queue_waiter, node);

		waiter->item = sem;
		waiter->completed = true;
		bt_base_require(osi_sem_give(&waiter->wake));
	} else if (sem->count < sem->limit) {
		sem->count++;
	}
	queue_release();
}

unsigned int k_sem_count_get(struct k_sem *sem)
{
	unsigned int count;

	queue_acquire();
	count = sem->count;
	queue_release();
	return count;
}

void k_sem_reset(struct k_sem *sem)
{
	sys_snode_t *node;

	queue_acquire();
	sem->count = 0;
	while ((node = sys_slist_get(&sem->waiters)) != NULL) {
		struct queue_waiter *waiter = CONTAINER_OF(node, struct queue_waiter, node);

		waiter->completed = true;
		bt_base_require(osi_sem_give(&waiter->wake));
	}
	queue_release();
}
