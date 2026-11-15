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
 * @brief Internal OSI-backed mechanisms required by the imported Host.
 */

#ifndef EMBT_BASE_H_
#define EMBT_BASE_H_

#include <assert.h>
#include <string.h>
#include <embt/storage.h>
#include <embt/sys/__assert.h>
#include <base/bt_atomic.h>
#include <osi/osi.h>

/**
 * @brief Initialize process-lifetime base locks and the default worker.
 *
 * Call from the serialized stack lifecycle owner. No transport is opened.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_base_init(void);
/**
 * @brief Stop and join the default worker after clients quiesce.
 *
 * No queue, pool or work API may run concurrently with this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_base_deinit(void);
/**
 * @brief Terminate on an error that an inherited void API cannot report.
 *
 * Nonzero results are fatal; zero returns normally.
 *
 * @param[in] result Status to check.
 */
void bt_base_require(int result);
/**
 * @brief Read monotonic uptime in milliseconds.
 * @return Monotonic milliseconds.
 */
int64_t k_uptime_get(void);
/**
 * @brief Read the low 32 bits of monotonic uptime in milliseconds.
 *
 * The result wraps modulo 2^32.
 * @return Monotonic milliseconds modulo 2^32.
 */
uint32_t k_uptime_get_32(void);
/**
 * @brief Convert a relative wait into one absolute monotonic deadline.
 *
 * Saturates to the forever deadline if addition would overflow.
 *
 * @param[in] timeout Relative milliseconds; K_NO_WAIT or K_FOREVER when supported.
 * @return Absolute deadline, including the forever sentinel.
 */
k_timepoint_t sys_timepoint_calc(k_timeout_t timeout);
/**
 * @brief Compute the remaining wait without restarting its deadline.
 *
 * An expired deadline returns K_NO_WAIT.
 *
 * @param[in] end Absolute monotonic deadline.
 * @return Remaining relative timeout.
 */
k_timeout_t sys_timepoint_timeout(k_timepoint_t end);
/**
 * @brief Test whether a finite monotonic deadline has expired.
 *
 * @param[in] end Absolute monotonic deadline.
 * @return True if the condition holds, otherwise false.
 */
bool sys_timepoint_expired(k_timepoint_t end);
/**
 * @brief Suspend this thread for a relative interval in milliseconds.
 *
 * Uses the OSI monotonic sleep implementation.
 *
 * @param[in] ticks Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_sleep(k_timeout_t timeout)
{
	return osi_sleep_ms(timeout.ticks);
}
/**
 * @brief Yield execution to the POSIX scheduler.
 */
static inline void k_yield(void)
{
	bt_base_require(osi_thread_yield());
}
/**
 * @brief Report the execution context of this thread-only port.
 *
 * Always returns false: no Host code executes in an ISR or signal handler.
 * @return True if the condition holds, otherwise false.
 */
static inline bool k_is_in_isr(void)
{
	return false;
}
/**
 * @brief Terminate after an unrecoverable stack invariant failure.
 *
 * This function does not return.
 */
static inline void k_oops(void)
{
	osi_fatal("EmBT invariant failure");
}
typedef os_tid_t k_tid_t;
/**
 * @brief Return the identity of the calling OSI thread.
 * @return Current thread identity.
 */
static inline k_tid_t k_current_get(void)
{
	return osi_thread_self();
}

struct k_mutex {
	os_mutex_t native;
};
struct k_condvar {
	os_cond_t native;
};

/**
 * @brief Initialize a recursive mutex through OSI.
 *
 * The caller supplies uninitialized, exclusively owned storage.
 *
 * @param[in] native Value used by this operation.
 * @param[in] OS_MUTEX_RECURSIVE Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_mutex_init(struct k_mutex *m)
{
	return osi_mutex_init(&m->native, OS_MUTEX_RECURSIVE);
}
/**
 * @brief Acquire a recursive mutex with a relative timeout.
 *
 * A successful lock must be matched by one unlock.
 *
 * @param[in,out] m Mutex storage.
 * @param[in] native Value used by this operation.
 * @param[in] ticks Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_mutex_lock(struct k_mutex *m, k_timeout_t t)
{
	return osi_mutex_lock(&m->native, t.ticks);
}
/**
 * @brief Release one recursive acquisition of a mutex.
 *
 * The caller must own the mutex.
 *
 * @param[in] native Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_mutex_unlock(struct k_mutex *m)
{
	return osi_mutex_unlock(&m->native);
}
/**
 * @brief Initialize a monotonic OSI condition variable.
 *
 * @param[in] native Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_condvar_init(struct k_condvar *c)
{
	return osi_cond_init(&c->native);
}
/**
 * @brief Wait for a condition while releasing and reacquiring its mutex.
 *
 * The mutex must be owned exactly once; recheck the predicate after wakeup.
 *
 * @param[in,out] c Condition variable storage.
 * @param[in,out] m Mutex storage.
 * @param[in] native Value used by this operation.
 * @param[in] native Value used by this operation.
 * @param[in] ticks Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_condvar_wait(struct k_condvar *c, struct k_mutex *m, k_timeout_t t)
{
	return osi_cond_wait(&c->native, &m->native, t.ticks);
}
/**
 * @brief Wake all current condition waiters.
 *
 * @param[in] native Value used by this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_condvar_broadcast(struct k_condvar *c)
{
	return osi_cond_broadcast(&c->native);
}
/**
 * @brief Read the fixed maximum credit count.
 *
 * The semaphore must not be concurrently reinitialized.
 *
 * @param[in] s Initialized semaphore.
 * @return Maximum credit count.
 */
static inline unsigned int bt_sem_limit_get(struct k_sem *s)
{
	return s->limit;
}
/**
 * @brief Initialize a quiescent bounded semaphore.
 *
 * No native object is embedded; all earlier waiters must have finished.
 *
 * @param[in,out] sem Semaphore storage.
 * @param[in] count Initial credits, no greater than limit.
 * @param[in] limit Nonzero maximum credit count.
 * @return Zero on success, otherwise a negative errno value.
 */
int k_sem_init(struct k_sem *sem, unsigned int count, unsigned int limit);
/**
 * @brief Consume one credit, waiting up to one monotonic deadline.
 *
 * Reset wakes current waiters with -EAGAIN. Native thread cancellation is not supported inside
 * waits.
 *
 * @param[in,out] sem Semaphore storage.
 * @param[in] timeout Relative milliseconds; K_NO_WAIT or K_FOREVER when supported.
 * @return Zero on success, -EAGAIN on unavailable, reset or timeout, or another negative error.
 */
int k_sem_take(struct k_sem *sem, k_timeout_t timeout);
/**
 * @brief Give to the oldest waiter or increase credits up to the limit.
 *
 * Giving to a full semaphore leaves its count unchanged.
 *
 * @param[in,out] sem Semaphore storage.
 */
void k_sem_give(struct k_sem *sem);
/**
 * @brief Return a synchronized snapshot of available credits.
 *
 * @param[in,out] sem Semaphore storage.
 * @return Current available credits.
 */
unsigned int k_sem_count_get(struct k_sem *sem);
/**
 * @brief Clear credits and cancel all current waiters.
 *
 * Future waits are unaffected by the cancellation.
 *
 * @param[in,out] sem Semaphore storage.
 */
void k_sem_reset(struct k_sem *sem);
#define K_SEM_MAX_LIMIT INT_MAX

typedef unsigned int k_spinlock_key_t;
/**
 * @brief Acquire a short internal lock with acquire ordering.
 *
 * Do not sleep, invoke callbacks, or acquire this lock recursively while holding it.
 *
 * @param[in,out] lock Naturally aligned lock storage.
 * @return Token for the matching unlock.
 */
k_spinlock_key_t k_spin_lock(struct k_spinlock *lock);
/**
 * @brief Release a short internal lock with release ordering.
 *
 * @param[in,out] lock Naturally aligned lock storage.
 * @param[in] key Token returned by the matching lock call.
 */
void k_spin_unlock(struct k_spinlock *lock, k_spinlock_key_t key);
/**
 * @brief Acquire the shared recursive critical-section mutex.
 *
 * This protects only participating callers; it does not disable POSIX scheduling.
 */
void k_sched_lock(void);
/**
 * @brief Release one shared critical-section acquisition.
 */
void k_sched_unlock(void);

/**
 * @brief Initialize an empty intrusive queue.
 *
 * The caller guarantees no users or waiters of the previous state.
 *
 * @param[in,out] queue Queue owned by the caller.
 */
void k_queue_init(struct k_queue *queue);
/**
 * @brief Append an item or transfer it to the oldest waiter.
 *
 * The item starts with a sys_snode_t and must not already be linked in a queue.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] item Intrusive item whose ownership is transferred or queried.
 */
void k_queue_append(struct k_queue *queue, void *item);
/**
 * @brief Prepend an item or transfer it to the oldest waiter.
 *
 * The item starts with a sys_snode_t and must not already be linked in a queue.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] item Intrusive item whose ownership is transferred or queried.
 */
void k_queue_prepend(struct k_queue *queue, void *item);
/**
 * @brief Remove a queued item by its address.
 *
 * This cannot revoke ownership already transferred to a consumer.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] item Intrusive item whose ownership is transferred or queried.
 * @return True if the condition holds, otherwise false.
 */
bool k_queue_remove(struct k_queue *queue, void *item);
/** @brief Borrow the head under the queue lock; caller must protect item lifetime. */
void *k_queue_peek_head(struct k_queue *queue);
/** @brief Take an item with one monotonic deadline; NULL means empty, cancelled or expired. */
void *k_queue_get(struct k_queue *queue, k_timeout_t timeout);
/**
 * @brief Cancel the oldest current waiter.
 *
 * Future waits are unaffected; a successful cancellation returns NULL to that waiter.
 *
 * @param[in,out] queue Queue owned by the caller.
 */
void k_queue_cancel_wait(struct k_queue *queue);
/**
 * @brief Perform the init operation on this intrusive queue.
 *
 * Uses the corresponding k_queue operation and its ownership and wait contract.
 *
 * @param[in] _queue Value used by this operation.
 */
static inline void k_fifo_init(struct k_fifo *q)
{
	k_queue_init(&q->_queue);
}
/**
 * @brief Perform the put operation on this intrusive queue.
 *
 * Uses the corresponding k_queue operation and its ownership and wait contract.
 *
 * @param[in,out] q Initialized queue.
 * @param[in] _queue Value used by this operation.
 * @param[in,out] p Intrusive item.
 */
static inline void k_fifo_put(struct k_fifo *q, void *p)
{
	k_queue_append(&q->_queue, p);
}
static inline void *k_fifo_get(struct k_fifo *q, k_timeout_t t)
{
	return k_queue_get(&q->_queue, t);
}
static inline void *k_fifo_peek_head(struct k_fifo *q)
{
	return k_queue_peek_head(&q->_queue);
}
/**
 * @brief Perform the is empty operation on this intrusive queue.
 *
 * Uses the corresponding k_queue operation and its ownership and wait contract.
 *
 * @param[in,out] q Initialized queue.
 * @return True if the condition holds, otherwise false.
 */
static inline bool k_fifo_is_empty(struct k_fifo *q)
{
	return k_fifo_peek_head(q) == NULL;
}
/**
 * @brief Perform the cancel wait operation on this intrusive queue.
 *
 * Uses the corresponding k_queue operation and its ownership and wait contract.
 *
 * @param[in] _queue Value used by this operation.
 */
static inline void k_fifo_cancel_wait(struct k_fifo *q)
{
	k_queue_cancel_wait(&q->_queue);
}
/**
 * @brief Perform the init operation on this intrusive queue.
 *
 * Uses the corresponding k_queue operation and its ownership and wait contract.
 *
 * @param[in] _queue Value used by this operation.
 */
static inline void k_lifo_init(struct k_lifo *q)
{
	k_queue_init(&q->_queue);
}
/**
 * @brief Perform the put operation on this intrusive queue.
 *
 * Uses the corresponding k_queue operation and its ownership and wait contract.
 *
 * @param[in,out] q Initialized queue.
 * @param[in] _queue Value used by this operation.
 * @param[in,out] p Intrusive item.
 */
static inline void k_lifo_put(struct k_lifo *q, void *p)
{
	k_queue_prepend(&q->_queue, p);
}
static inline void *k_lifo_get(struct k_lifo *q, k_timeout_t t)
{
	return k_queue_get(&q->_queue, t);
}
#define K_FIFO_DEFINE(name) struct k_fifo name

/** @brief Fixed storage pool; blocks contain an intrusive free link while unused. */
struct k_mem_slab {
	struct k_queue free;
	void *storage;
	size_t block_size;
	size_t count;
};
#define K_MEM_SLAB_DEFINE_STATIC(name, size, count_, align_)                                       \
	static unsigned char name##_storage[(size) * (count_)] __aligned(align_);                  \
	static struct k_mem_slab name = {                                                          \
		.storage = name##_storage, .block_size = (size), .count = (count_)}
#define K_MEM_SLAB_DEFINE_STATIC_TYPE(name, type, count)                                           \
	K_MEM_SLAB_DEFINE_STATIC(name, sizeof(type), count, __alignof__(type))
/**
 * @brief Initialize all blocks of a fixed static pool.
 *
 * Requires exclusive ownership; no blocks may still be allocated.
 *
 * @param[in,out] slab Fixed block pool.
 */
void bt_mem_slab_init(struct k_mem_slab *slab);
/**
 * @brief Allocate a fixed-size block from a static pool.
 *
 * The output is NULL on failure. Release successful allocations to the same pool.
 *
 * @param[in,out] slab Fixed block pool.
 * @param[out] out Receives the allocation or NULL.
 * @param[in] timeout Relative milliseconds; K_NO_WAIT or K_FOREVER when supported.
 * @return Zero on success, -ENOMEM on a nonblocking miss, or -EAGAIN after a wait fails.
 */
int k_mem_slab_alloc(struct k_mem_slab *slab, void **out, k_timeout_t timeout);
/**
 * @brief Return a block to its owning static pool.
 *
 * The block must have been allocated from this pool and must not have been freed.
 *
 * @param[in,out] slab Fixed block pool.
 * @param[in] block Allocation to return.
 */
void k_mem_slab_free(struct k_mem_slab *slab, void *block);

/** @brief Allocate from a bounded static arena with a monotonic timeout. */
void *k_heap_aligned_alloc(struct k_heap *heap, size_t alignment, size_t size, k_timeout_t timeout);
static inline void *k_heap_alloc(struct k_heap *heap, size_t size, k_timeout_t timeout)
{
	return k_heap_aligned_alloc(heap, sizeof(void *), size, timeout);
}
/**
 * @brief Return an allocation and coalesce adjacent free arena blocks.
 *
 * The allocation must belong to this arena. NULL is accepted.
 *
 * @param[in,out] heap Bounded arena descriptor.
 * @param[in] block Allocation to return.
 */
void k_heap_free(struct k_heap *heap, void *block);

#define K_WORK_RUNNING   BIT(0)
#define K_WORK_QUEUED    BIT(1)
#define K_WORK_DELAYED   BIT(2)
#define K_WORK_CANCELING BIT(3)
struct bt_work_runtime;
struct k_work_q {
	struct bt_work_runtime *runtime;
};
struct k_work_queue_config {
	const char *name;
	bool no_yield;
};
extern struct k_work_q k_sys_work_q;
#define Z_WORK_INITIALIZER(fn)                                                                     \
	{                                                                                          \
		.handler = (fn)                                                                    \
	}
#define K_WORK_DEFINE(name, fn)           struct k_work name = {.handler = (fn)}
#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.work = {.handler = (fn)}}
/**
 * @brief Initialize idle caller-owned work storage.
 *
 * The work must not be queued, delayed, or running.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in] handler Callback invoked without the internal work lock.
 */
void k_work_init(struct k_work *work, void (*handler)(struct k_work *));
/**
 * @brief Initialize idle work and its deadline storage.
 *
 * The object owns no native timer or heap allocation.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in] handler Callback invoked without the internal work lock.
 */
void k_work_init_delayable(struct k_work_delayable *work, void (*handler)(struct k_work *));
/**
 * @brief Initialize queue storage before starting its worker.
 *
 * The queue must not already own a runtime.
 *
 * @param[in,out] queue Queue owned by the caller.
 */
void k_work_queue_init(struct k_work_q *queue);
/**
 * @brief Start an explicitly initialized queue.
 *
 * Lifecycle calls are serialized by the owner. Uses normal POSIX scheduling; failure releases
 * partially created resources.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in] name Optional worker name.
 * @param[in] stack_size Native stack size in bytes; zero selects the OS default.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_queue_start(struct k_work_q *queue, const char *name, size_t stack_size);
/**
 * @brief Cancel pending work and join the worker.
 *
 * The owner stops external submissions and synchronous waiters first. Returns -EDEADLK from the
 * queue worker.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_queue_stop(struct k_work_q *queue);
/**
 * @brief Return the identity of a started queue worker.
 *
 * The owner keeps the queue started during the call.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @return Worker thread identity.
 */
k_tid_t k_work_queue_thread_get(struct k_work_q *queue);
/**
 * @brief Submit ordinary work to a started queue.
 *
 * A running work item stays on its current queue, preventing concurrent execution of one handler.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] work Caller-owned work storage.
 * @return One for a new submission, two for a running resubmission, zero if queued, or negative
 * errno.
 */
int k_work_submit_to_queue(struct k_work_q *queue, struct k_work *work);
/**
 * @brief Submit or schedule work on the default system queue.
 *
 * Uses the corresponding queue-specific work contract.
 *
 * @param[in] k_sys_work_q Value used by this operation.
 * @param[in,out] work Caller-owned work storage.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_work_submit(struct k_work *work)
{
	return k_work_submit_to_queue(&k_sys_work_q, work);
}
/**
 * @brief Schedule work if it is not already pending.
 *
 * Uses the queue worker and an absolute monotonic deadline; no native timer callback can outlive
 * the work.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] work Caller-owned work storage.
 * @param[in] timeout Relative milliseconds; K_NO_WAIT or K_FOREVER when supported.
 * @return One if scheduled, zero if already pending, or negative errno.
 */
int k_work_schedule_for_queue(struct k_work_q *queue, struct k_work_delayable *work,
			      k_timeout_t timeout);
/**
 * @brief Replace a pending deadline and schedule delayed work.
 *
 * An already queued invocation remains queued, matching the imported work contract.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] work Caller-owned work storage.
 * @param[in] timeout Relative milliseconds; K_NO_WAIT or K_FOREVER when supported.
 * @return Positive if scheduled, zero if already queued, or negative errno.
 */
int k_work_reschedule_for_queue(struct k_work_q *queue, struct k_work_delayable *work,
				k_timeout_t timeout);
/**
 * @brief Submit or schedule work on the default system queue.
 *
 * Uses the corresponding queue-specific work contract.
 *
 * @param[in,out] w Caller-owned work storage.
 * @param[in] k_sys_work_q Value used by this operation.
 * @param[in,out] w Caller-owned work storage.
 * @param[in] t Relative timeout with K_NO_WAIT and K_FOREVER support.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_work_schedule(struct k_work_delayable *w, k_timeout_t t)
{
	return k_work_schedule_for_queue(&k_sys_work_q, w, t);
}
/**
 * @brief Submit or schedule work on the default system queue.
 *
 * Uses the corresponding queue-specific work contract.
 *
 * @param[in,out] w Caller-owned work storage.
 * @param[in] k_sys_work_q Value used by this operation.
 * @param[in,out] w Caller-owned work storage.
 * @param[in] t Relative timeout with K_NO_WAIT and K_FOREVER support.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline int k_work_reschedule(struct k_work_delayable *w, k_timeout_t t)
{
	return k_work_reschedule_for_queue(&k_sys_work_q, w, t);
}
/**
 * @brief Remove pending work and request cancellation of a running invocation.
 *
 * Does not wait. A running handler cannot resubmit until it returns. Storage must remain alive
 * until completion.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Remaining busy flags after cancellation.
 */
int k_work_cancel(struct k_work *work);
/**
 * @brief Remove a deadline and cancel ordinary work.
 *
 * Does not wait for a running callback.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Remaining busy flags after cancellation.
 */
int k_work_cancel_delayable(struct k_work_delayable *work);
/**
 * @brief Cancel work and wait for its running callback.
 *
 * Do not call from the owning worker. Keep the work storage alive until return.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in,out] sync Caller-owned compatibility token, not retained after return.
 * @return True if the condition holds, otherwise false.
 */
bool k_work_cancel_sync(struct k_work *work, struct k_work_sync *sync);
/**
 * @brief Cancel the deadline and wait for callback completion.
 *
 * After return, no queued or timed reference remains. External resubmission must be excluded by the
 * caller.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in,out] sync Caller-owned compatibility token, not retained after return.
 * @return True if the condition holds, otherwise false.
 */
bool k_work_cancel_delayable_sync(struct k_work_delayable *work, struct k_work_sync *sync);
/**
 * @brief Wait for the submission observed on entry.
 *
 * Later submissions do not prolong the wait. Do not call from the owning worker.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in,out] sync Caller-owned compatibility token, not retained after return.
 * @return True if the condition holds, otherwise false.
 */
bool k_work_flush(struct k_work *work, struct k_work_sync *sync);
/**
 * @brief Read a synchronized snapshot of queued, running and cancelling flags.
 *
 * The snapshot is not a lifetime barrier.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return A K_WORK_* bit mask.
 */
unsigned int k_work_busy_get(const struct k_work *work);
/**
 * @brief Query the ordinary work state or containing deadline object.
 *
 * The caller keeps the work storage alive; queries are snapshots.
 *
 * @param[in,out] w Caller-owned work storage.
 * @return True if the condition holds, otherwise false.
 */
static inline bool k_work_is_pending(const struct k_work *w)
{
	return k_work_busy_get(w) != 0;
}
/**
 * @brief Query the ordinary work state or containing deadline object.
 *
 * The caller keeps the work storage alive; queries are snapshots.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Zero on success, otherwise a negative errno value.
 */
static inline unsigned int k_work_delayable_busy_get(const struct k_work_delayable *w)
{
	return k_work_busy_get(&w->work);
}
/**
 * @brief Read remaining milliseconds before a pending deadline.
 *
 * Returns zero when no deadline is pending.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Remaining milliseconds, or zero.
 */
int64_t k_work_delayable_remaining_get(const struct k_work_delayable *work);
static inline struct k_work_delayable *k_work_delayable_from_work(struct k_work *w)
{
	return CONTAINER_OF(w, struct k_work_delayable, work);
}

#endif /* EMBT_BASE_H_ */
