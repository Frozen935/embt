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
 * @brief Serialized work queues and monotonic delayed work.
 */

#ifndef EMBT_BASE_WORK_H_
#define EMBT_BASE_WORK_H_

#include <stdint.h>
#include <stdbool.h>
#include <embt/base/slist.h>
#include <embt/base/time.h>
#include <embt/base/util.h>
#include <embt/osi/sync.h>
#include <embt/osi/thread.h>

struct bt_work_q;
struct bt_work;
typedef void (*bt_work_handler_t)(struct bt_work *work);

/** @brief Native-style deferred work storage; keep alive until execution finishes. */
struct bt_work {
	sys_snode_t node;
	bt_work_handler_t handler;
	struct bt_work_q *queue;
	uint32_t flags;
};

/** @brief Physical timeout record owned by the shared work timeout provider. */
struct bt_work_timeout {
	sys_snode_t node;
	int64_t deadline_ms;
	bool scheduled;
};

/** @brief Delayed submission target, distinct from the current pending queue. */
struct bt_work_delayable {
	struct bt_work work;
	struct bt_work_timeout timeout;
	struct bt_work_q *queue;
};

struct bt_work_flusher {
	struct bt_work work;
	os_sem_t sem;
};
struct bt_work_canceller {
	sys_snode_t node;
	struct bt_work *work;
	os_sem_t sem;
};
/** @brief One caller-owned synchronization record per concurrent operation. */
struct bt_work_sync {
	union {
		struct bt_work_flusher flusher;
		struct bt_work_canceller canceller;
	};
};


#define BT_WORK_RUNNING_BIT 0
#define BT_WORK_RUNNING BIT(BT_WORK_RUNNING_BIT)
#define BT_WORK_CANCELING_BIT 1
#define BT_WORK_CANCELING BIT(BT_WORK_CANCELING_BIT)
#define BT_WORK_QUEUED_BIT 2
#define BT_WORK_QUEUED BIT(BT_WORK_QUEUED_BIT)
#define BT_WORK_DELAYED_BIT 3
#define BT_WORK_DELAYED BIT(BT_WORK_DELAYED_BIT)
#define BT_WORK_FLUSHING_BIT 4
#define BT_WORK_FLUSHING BIT(BT_WORK_FLUSHING_BIT)
#define BT_WORK_DELAYABLE_BIT 8
#define BT_WORK_DELAYABLE BIT(BT_WORK_DELAYABLE_BIT)
#define BT_WORK_MASK (BT_WORK_RUNNING | BT_WORK_CANCELING | BT_WORK_QUEUED | BT_WORK_DELAYED | BT_WORK_FLUSHING)
#define BT_WORK_QUEUE_STARTED_BIT 0
#define BT_WORK_QUEUE_STARTED BIT(BT_WORK_QUEUE_STARTED_BIT)
#define BT_WORK_QUEUE_BUSY_BIT 1
#define BT_WORK_QUEUE_BUSY BIT(BT_WORK_QUEUE_BUSY_BIT)
#define BT_WORK_QUEUE_DRAIN_BIT 2
#define BT_WORK_QUEUE_DRAIN BIT(BT_WORK_QUEUE_DRAIN_BIT)
#define BT_WORK_QUEUE_PLUGGED_BIT 3
#define BT_WORK_QUEUE_PLUGGED BIT(BT_WORK_QUEUE_PLUGGED_BIT)
#define BT_WORK_QUEUE_STOP_BIT 4
#define BT_WORK_QUEUE_STOP BIT(BT_WORK_QUEUE_STOP_BIT)
#define BT_WORK_QUEUE_NO_YIELD_BIT 8
#define BT_WORK_QUEUE_NO_YIELD BIT(BT_WORK_QUEUE_NO_YIELD_BIT)

struct bt_work_q {
	os_thread_t thread;
	os_sem_t notifyq;
	sys_slist_t pending;
	sys_slist_t drainers;
	uint32_t flags;
	bool initialized;
	bool started;
	bool joinable;
};
struct bt_work_queue_config {
	const char *name;
	bool no_yield;
};
extern struct bt_work_q bt_sys_work_q;
#define BT_WORK_INITIALIZER(fn)                                                                     \
	{                                                                                          \
		.handler = (fn)                                                                    \
	}
#define BT_WORK_DEFINE(name, fn)           struct bt_work name = {.handler = (fn)}
#define BT_WORK_DELAYABLE_INITIALIZER(fn) {.work = {.handler = (fn), .flags = BT_WORK_DELAYABLE}}
#define BT_WORK_DELAYABLE_DEFINE(name, fn) struct bt_work_delayable name = BT_WORK_DELAYABLE_INITIALIZER(fn)
/**
 * @brief Initialize idle caller-owned work storage.
 *
 * The work must not be queued, delayed, or running.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in] handler Callback invoked without the internal work lock.
 */
void bt_work_init(struct bt_work *work, bt_work_handler_t handler);
/**
 * @brief Initialize idle delayable work and its timeout record.
 *
 * The object owns an embedded timeout record, without a per-item OS timer.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in] handler Callback invoked without the internal work lock.
 */
void bt_work_init_delayable(struct bt_work_delayable *work, bt_work_handler_t handler);
/**
 * @brief Initialize queue storage before starting its worker.
 *
 * Zero storage before the first call. The owner serializes lifecycle calls.
 * A stopped, initialized queue retains its notification semaphore for restart.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @return Zero on success, -EALREADY while started, or a negative native error.
 */
int bt_work_queue_init(struct bt_work_q *queue);
/**
 * @brief Destroy synchronization storage after stopping the worker.
 * @param[in,out] queue Caller-owned queue with no users or synchronous waiters.
 * @return Zero on success, -EBUSY while started, or a negative native error.
 * @note Queue storage must outlive every work item that still refers to it.
 */
int bt_work_queue_deinit(struct bt_work_q *queue);
/**
 * @brief Start an explicitly initialized queue.
 *
 * Lifecycle calls are serialized by the owner. Uses normal POSIX scheduling; failure releases
 * partially created resources.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in] priority OSI thread priority; POSIX currently accepts zero.
 * @param[in] config Optional worker name and no-yield policy.
 * @param[in] stack_size Native stack size in bytes; zero selects the OS default.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_queue_start(struct bt_work_q *queue, size_t stack_size, int priority,
			const struct bt_work_queue_config *config);
/**
 * @brief Stop a plugged and drained worker, retaining initialized storage.
 *
 * Call bt_work_queue_drain(queue, true) first. Cancel delayable work separately.
 * Returns -EBUSY if not plugged and -EDEADLK from the worker. OSI join has no timeout.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_queue_stop(struct bt_work_q *queue);
/**
 * @brief Wait for queued work and chained submissions to finish.
 * @param[in,out] queue Started queue; do not call from its worker.
 * @param[in] plug Reject future submissions until unplugged.
 * @return Zero when drained, -ENODEV if not started, or -EDEADLK from worker.
 * @note Delayed submissions are not included; cancel them before deinit.
 */
int bt_work_queue_drain(struct bt_work_q *queue, bool plug);
/** @brief Permit submissions again; returns -EALREADY if not plugged. */
int bt_work_queue_unplug(struct bt_work_q *queue);
/**
 * @brief Borrow the OSI thread handle of a started queue worker.
 *
 * The owner keeps the queue started during the call.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @return Worker handle, valid until the queue is stopped.
 */
os_thread_t *bt_work_queue_thread_get(struct bt_work_q *queue);
/**
 * @brief Submit ordinary work to a started queue.
 *
 * A running work item stays on its current queue, preventing concurrent execution of one handler.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] work Caller-owned work storage.
 * @return One for a new submission, two for a running resubmission, zero if queued,
 * -EINVAL if no current/target queue exists, -ENODEV if not started, or -EBUSY
 * while cancelling, draining or plugged. NULL queue resubmits to the last queue.
 */
int bt_work_submit_to_queue(struct bt_work_q *queue, struct bt_work *work);
/**
 * @brief Submit or schedule work on the default system queue.
 *
 * Uses the corresponding queue-specific work contract.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_submit(struct bt_work *work);
/**
 * @brief Schedule work if it is not already pending.
 *
 * The shared timeout provider submits the item when due, independently of
 * the queue worker. Cancel or flush delayed work before releasing its storage.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] work Caller-owned work storage.
 * @param[in] timeout Relative milliseconds; BT_NO_WAIT or BT_FOREVER when supported.
 * @return One if scheduled, zero if already pending, or negative errno.
 */
int bt_work_schedule_for_queue(struct bt_work_q *queue, struct bt_work_delayable *work,
			      bt_timeout_t timeout);
/**
 * @brief Replace a pending deadline and schedule delayed work.
 *
 * An already queued invocation remains queued, matching the imported work contract.
 *
 * @param[in,out] queue Queue owned by the caller.
 * @param[in,out] work Caller-owned work storage.
 * @param[in] timeout Relative milliseconds; BT_NO_WAIT or BT_FOREVER when supported.
 * @return One for a nonzero delayed schedule. BT_NO_WAIT uses submission status.
 * @note A current queued instance stays on its queue; the delayed target is separate.
 */
int bt_work_reschedule_for_queue(struct bt_work_q *queue, struct bt_work_delayable *work,
				bt_timeout_t timeout);
/**
 * @brief Submit or schedule work on the default system queue.
 *
 * Uses the corresponding queue-specific work contract.
 *
 * @param[in,out] w Caller-owned work storage.
 * @param[in] t Relative timeout with BT_NO_WAIT and BT_FOREVER support.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_schedule(struct bt_work_delayable *w, bt_timeout_t t);
/**
 * @brief Submit or schedule work on the default system queue.
 *
 * Uses the corresponding queue-specific work contract.
 *
 * @param[in,out] w Caller-owned work storage.
 * @param[in] t Relative timeout with BT_NO_WAIT and BT_FOREVER support.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_work_reschedule(struct bt_work_delayable *w, bt_timeout_t t);
/**
 * @brief Remove pending work and request cancellation of a running invocation.
 *
 * Does not wait. A running handler cannot resubmit until it returns. Storage must remain alive
 * until completion.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Remaining busy flags after cancellation.
 */
int bt_work_cancel(struct bt_work *work);
/**
 * @brief Remove a deadline and cancel ordinary work.
 *
 * Does not wait for a running callback.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Remaining busy flags after cancellation.
 */
int bt_work_cancel_delayable(struct bt_work_delayable *work);
/**
 * @brief Cancel work and wait for its running callback.
 *
 * Do not call from the owning worker. Keep the work storage alive until return.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in,out] sync Caller-owned native-style semaphore record; do not reuse concurrently.
 * @return True if the condition holds, otherwise false.
 */
bool bt_work_cancel_sync(struct bt_work *work, struct bt_work_sync *sync);
/**
 * @brief Cancel the deadline and wait for callback completion.
 *
 * After return, no queued or timed reference remains. External resubmission must be excluded by the
 * caller.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in,out] sync Caller-owned native-style semaphore record; do not reuse concurrently.
 * @return True if the condition holds, otherwise false.
 */
bool bt_work_cancel_delayable_sync(struct bt_work_delayable *work, struct bt_work_sync *sync);
/**
 * @brief Wait for the submission observed on entry.
 *
 * A flusher marker is inserted after the currently queued instance. Later
 * resubmissions do not prolong the wait. Use flush_delayable for delayed items.
 * Do not call from the owning worker.
 *
 * @param[in,out] work Caller-owned work storage.
 * @param[in,out] sync Caller-owned native-style semaphore record; do not reuse concurrently.
 * @return True if the condition holds, otherwise false.
 */
bool bt_work_flush(struct bt_work *work, struct bt_work_sync *sync);
/**
 * @brief Submit delayed work immediately if necessary and flush that instance.
 * @param[in,out] work Live delayable work, not owned by the calling worker.
 * @param[in,out] sync Caller-owned record, valid until return.
 * @return True if it waited for a queued or running instance, false if idle.
 */
bool bt_work_flush_delayable(struct bt_work_delayable *work, struct bt_work_sync *sync);
/**
 * @brief Read a synchronized snapshot of queued, running and cancelling flags.
 *
 * The snapshot is not a lifetime barrier.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return A BT_WORK_* bit mask.
 */
unsigned int bt_work_busy_get(const struct bt_work *work);
/**
 * @brief Query the ordinary work state or containing deadline object.
 *
 * The caller keeps the work storage alive; queries are snapshots.
 *
 * @param[in,out] w Caller-owned work storage.
 * @return True if the condition holds, otherwise false.
 */
static inline bool bt_work_is_pending(const struct bt_work *w)
{
	return bt_work_busy_get(w) != 0;
}
/**
 * @brief Query the ordinary work state or containing deadline object.
 *
 * The caller keeps the work storage alive; queries are snapshots.
 *
 * @param[in] w Caller-owned delayed work storage.
 * @return A BT_WORK_* bit mask.
 */
int bt_work_delayable_busy_get(const struct bt_work_delayable *work);
/**
 * @brief Test whether delayed work is queued, running, or scheduled.
 * @param[in] work Caller-owned delayed work storage.
 * @return True if any work state is active; this is not a lifetime barrier.
 */
static inline bool bt_work_delayable_is_pending(const struct bt_work_delayable *work)
{
	return bt_work_delayable_busy_get(work) != 0;
}

/**
 * @brief Read remaining milliseconds before a pending deadline.
 *
 * Returns zero when no deadline is pending.
 *
 * @param[in,out] work Caller-owned work storage.
 * @return Remaining milliseconds, or zero.
 */
int64_t bt_work_delayable_remaining_get(const struct bt_work_delayable *work);
static inline struct bt_work_delayable *bt_work_delayable_from_work(struct bt_work *w)
{
	return CONTAINER_OF(w, struct bt_work_delayable, work);
}

#endif /* EMBT_BASE_WORK_H_ */
