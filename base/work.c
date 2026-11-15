/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 * Copyright (C) 2026 EmBT contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * Second generation work queue implementation
 *
 * Ported from Zephyr f8da8ea370508f274685a25ff19dbe5a9ff54790.
 * Native work state transitions are retained; OSI supplies mutexes, semaphores
 * and threads. A shared timeout thread replaces the kernel timeout provider.
 */

#include <embt/base/work.h>
#include <embt/base/base.h>
#include <embt/base/time.h>
#include <embt/osi/sync.h>
#include <embt/osi/thread.h>
#include <embt/base/assert.h>
#include <errno.h>

static inline void flag_clear(uint32_t *flagp,
			      uint32_t bit)
{
	*flagp &= ~BIT(bit);
}

static inline void flag_set(uint32_t *flagp,
			    uint32_t bit)
{
	*flagp |= BIT(bit);
}

static inline bool flag_test(const uint32_t *flagp,
			     uint32_t bit)
{
	return (*flagp & BIT(bit)) != 0U;
}

static inline bool flag_test_and_clear(uint32_t *flagp,
				       int bit)
{
	bool ret = flag_test(flagp, bit);

	flag_clear(flagp, bit);

	return ret;
}

static inline void flags_set(uint32_t *flagp,
			     uint32_t flags)
{
	*flagp = flags;
}

static inline uint32_t flags_get(const uint32_t *flagp)
{
	return *flagp;
}

/* Lock to protect the internal state of all work items, work queues,
 * and pending_cancels.
 */
static os_mutex_t work_lock;
static bool work_lock_initialized;
static unsigned int initialized_queues;
static bool timeout_started;
static bool timeout_stopping;
static os_thread_t timeout_thread;
static os_sem_t timeout_notify;
static sys_slist_t timeouts;

struct bt_work_q bt_sys_work_q;

struct drain_waiter {
	sys_snode_t node;
	os_sem_t sem;
};

static void timeout_main(void *arg);
static void work_timeout_locked(struct bt_work_timeout *record);

/* Wake ownership is serialized with the timeout callback by work_lock. */
static void timeout_add_locked(struct bt_work_timeout *record, bt_timeout_t delay)
{
	sys_snode_t *previous = NULL;
	sys_snode_t *node;

	record->deadline_ms = bt_timepoint_calc(delay).deadline_ms;
	record->scheduled = true;
	SYS_SLIST_FOR_EACH_NODE(&timeouts, node) {
		struct bt_work_timeout *entry = CONTAINER_OF(node, struct bt_work_timeout, node);

		if (entry->deadline_ms > record->deadline_ms) {
			break;
		}
		previous = node;
	}
	sys_slist_insert(&timeouts, previous, &record->node);
	bt_base_require(osi_sem_give(&timeout_notify));
}

static void timeout_abort_locked(struct bt_work_timeout *record)
{
	if (record->scheduled) {
		sys_slist_find_and_remove(&timeouts, &record->node);
		record->scheduled = false;
		bt_base_require(osi_sem_give(&timeout_notify));
	}
}

/* Semaphore wait completion must also wait for the giver to leave work_lock. */
static void wait_completion(os_sem_t *sem)
{
	bt_base_require(osi_sem_take(sem, OS_TIMEOUT_FOREVER));
	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	bt_base_require(osi_mutex_unlock(&work_lock));
	bt_base_require(osi_sem_deinit(sem));
}

static void drain_wake_all_locked(struct bt_work_q *queue)
{
	sys_snode_t *node;

	while ((node = sys_slist_get(&queue->drainers)) != NULL) {
		struct drain_waiter *waiter = CONTAINER_OF(node, struct drain_waiter, node);

		bt_base_require(osi_sem_give(&waiter->sem));
	}
}

/* Invoked by work thread */
static void handle_flush(struct bt_work *work)
{
	(void)work;
}

static inline void init_flusher(struct bt_work_flusher *flusher)
{
	struct bt_work *work = &flusher->work;
	bt_base_require(osi_sem_init(&flusher->sem, 0, 1));
	bt_work_init(&flusher->work, handle_flush);
	flag_set(&work->flags, BT_WORK_FLUSHING_BIT);
}

/* List of pending cancellations. */
static sys_slist_t pending_cancels;

/* Initialize a canceler record and add it to the list of pending
 * cancels.
 *
 * Invoked with work lock held.
 *
 * @param canceler the structure used to notify a waiting process.
 * @param work the work structure that is to be canceled
 */
static inline void init_work_cancel(struct bt_work_canceller *canceler,
				    struct bt_work *work)
{
	bt_base_require(osi_sem_init(&canceler->sem, 0, 1));
	canceler->work = work;
	sys_slist_append(&pending_cancels, &canceler->node);
}

/* Complete flushing of a work item.
 *
 * Invoked with work lock held.
 *
 * Invoked from a work queue thread.
 *
 * Notifies the waiting caller through OSI.
 *
 * @param work the work structure that has completed flushing.
 */
static void finalize_flush_locked(struct bt_work *work)
{
	struct bt_work_flusher *flusher
		= CONTAINER_OF(work, struct bt_work_flusher, work);

	flag_clear(&work->flags, BT_WORK_FLUSHING_BIT);

	bt_base_require(osi_sem_give(&flusher->sem));
};

/* Complete cancellation of a work item and unlock held lock.
 *
 * Invoked with work lock held.
 *
 * Invoked from a work queue thread.
 *
 * Reschedules.
 *
 * @param work the work structure that has completed cancellation
 */
static void finalize_cancel_locked(struct bt_work *work)
{
	struct bt_work_canceller *wc, *tmp;
	sys_snode_t *prev = NULL;

	/* Clear this first, so released high-priority threads don't
	 * see it when doing things.
	 */
	flag_clear(&work->flags, BT_WORK_CANCELING_BIT);

	/* Search for and remove matching containers, and release
	 * what's waiting for the completion.  The same work item can
	 * appear multiple times in the list if multiple threads
	 * attempt to cancel it; each waiter must be woken.
	 */
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&pending_cancels, wc, tmp, node) {
		if (wc->work == work) {
			sys_slist_remove(&pending_cancels, prev, &wc->node);
			bt_base_require(osi_sem_give(&wc->sem));
		} else {
			prev = &wc->node;
		}
	}
}

void bt_work_init(struct bt_work *work,
		  bt_work_handler_t handler)
{
	__ASSERT_NO_MSG(work != NULL);
	__ASSERT_NO_MSG(handler != NULL);

	*work = (struct bt_work)BT_WORK_INITIALIZER(handler);
}

static inline int work_busy_get_locked(const struct bt_work *work)
{
	return flags_get(&work->flags) & BT_WORK_MASK;
}

unsigned int bt_work_busy_get(const struct bt_work *work)
{

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	int ret = work_busy_get_locked(work);

	bt_base_require(osi_mutex_unlock(&work_lock));

	return ret;
}

/* Add a flusher work item to the queue.
 *
 * Invoked with work lock held.
 *
 * Caller must notify queue of pending work.
 *
 * @param queue queue on which a work item may appear.
 * @param work the work item that is either queued or running on @p
 * queue
 * @param flusher an uninitialized/unused flusher object
 */
static void queue_flusher_locked(struct bt_work_q *queue,
				 struct bt_work *work,
				 struct bt_work_flusher *flusher)
{
	init_flusher(flusher);

	if ((flags_get(&work->flags) & BT_WORK_QUEUED) != 0U) {
		sys_slist_insert(&queue->pending, &work->node,
				 &flusher->work.node);
	} else {
		sys_slist_prepend(&queue->pending, &flusher->work.node);
	}
}

/* Try to remove a work item from the given queue.
 *
 * Invoked with work lock held.
 *
 * @param queue the queue from which the work should be removed
 * @param work work that may be on the queue
 */
static inline void queue_remove_locked(struct bt_work_q *queue,
				       struct bt_work *work)
{
	if (flag_test_and_clear(&work->flags, BT_WORK_QUEUED_BIT)) {
		(void)sys_slist_find_and_remove(&queue->pending, &work->node);
	}
}

/* Potentially notify a queue that it needs to look for pending work.
 *
 * This may make the work queue thread ready, but as the lock is held it
 * will not be a reschedule point.  Callers should yield after the lock is
 * released where appropriate (generally if this returns true).
 *
 * @param queue to be notified.  If this is null no notification is required.
 *
 * @return true if and only if the queue was notified and woken, i.e. a
 * reschedule is pending.
 */
static inline bool notify_queue_locked(struct bt_work_q *queue)
{
	bool rv = false;

	if (queue != NULL) {
		bt_base_require(osi_sem_give(&queue->notifyq));
		rv = true;
	}

	return rv;
}

/* Submit an work item to a queue if queue state allows new work.
 *
 * Submission is rejected if no queue is provided, or if the queue is
 * draining and the work isn't being submitted from the queue's
 * thread (chained submission).
 *
 * Invoked with work lock held.
 * Conditionally notifies queue.
 *
 * @param queue the queue to which work should be submitted.  This may
 * be null, in which case the submission will fail.
 *
 * @param work to be submitted
 *
 * @retval 1 if successfully queued
 * @retval -EINVAL if no queue is provided
 * @retval -ENODEV if the queue is not started
 * @retval -EBUSY if the submission was rejected (draining, plugged)
 */
static inline int queue_submit_locked(struct bt_work_q *queue,
				      struct bt_work *work)
{
	if (queue == NULL) {
		return -EINVAL;
	}

	int ret;
	bool chained = queue->started && osi_thread_is_current(&queue->thread);
	bool draining = flag_test(&queue->flags, BT_WORK_QUEUE_DRAIN_BIT);
	bool plugged = flag_test(&queue->flags, BT_WORK_QUEUE_PLUGGED_BIT);

	/* Test for acceptability, in priority order:
	 *
	 * * -ENODEV if the queue isn't running.
	 * * -EBUSY if draining and not chained
	 * * -EBUSY if plugged and not draining
	 * * otherwise OK
	 */
	if (!flag_test(&queue->flags, BT_WORK_QUEUE_STARTED_BIT)) {
		ret = -ENODEV;
	} else if (draining && !chained) {
		ret = -EBUSY;
	} else if (plugged && !draining) {
		ret = -EBUSY;
	} else {
		sys_slist_append(&queue->pending, &work->node);
		ret = 1;
		(void)notify_queue_locked(queue);
	}

	return ret;
}

/* Attempt to submit work to a queue.
 *
 * The submission can fail if:
 * * the work is cancelling,
 * * no candidate queue can be identified;
 * * the candidate queue rejects the submission.
 *
 * Invoked with work lock held.
 * Conditionally notifies queue.
 *
 * @param work the work structure to be submitted

 * @param queuep pointer to a queue reference. On input this should
 * dereference to the proposed queue (which may be null); after completion it
 * will be null if the work was not submitted or if submitted will reference
 * the queue it was submitted to.  That may or may not be the queue provided
 * on input.
 *
 * @retval 0 if work was already submitted to a queue
 * @retval 1 if work was not submitted and has been queued to @p queue
 * @retval 2 if work was running and has been queued to the queue that was
 * running it
 * @retval -EBUSY if canceling or submission was rejected by queue
 * @retval -EINVAL if no queue is provided
 * @retval -ENODEV if the queue is not started
 */
static int submit_to_queue_locked(struct bt_work *work,
				  struct bt_work_q **queuep)
{
	int ret = 0;

	if (flag_test(&work->flags, BT_WORK_CANCELING_BIT)) {
		/* Disallowed */
		ret = -EBUSY;
	} else if (!flag_test(&work->flags, BT_WORK_QUEUED_BIT)) {
		/* Not currently queued */
		ret = 1;

		/* If no queue specified resubmit to last queue.
		 */
		if (*queuep == NULL) {
			*queuep = work->queue;
		}

		/* If the work is currently running we have to use the
		 * queue it's running on to prevent handler
		 * re-entrancy.
		 */
		if (flag_test(&work->flags, BT_WORK_RUNNING_BIT)) {
			__ASSERT_NO_MSG(work->queue != NULL);
			*queuep = work->queue;
			ret = 2;
		}

		int rc = queue_submit_locked(*queuep, work);

		if (rc < 0) {
			ret = rc;
		} else {
			flag_set(&work->flags, BT_WORK_QUEUED_BIT);
			work->queue = *queuep;
		}
	} else {
		/* Already queued, do nothing. */
	}

	if (ret <= 0) {
		*queuep = NULL;
	}

	return ret;
}

/* Submit work to a queue
 *
 * Intended for internal use.
 *
 * See also submit_to_queue_locked().
 *
 * @param queue pointer to the proposed queue.
 * @param work the work structure to be submitted
 *
 * @retval see submit_to_queue_locked()
 */
static int work_submit_to_queue(struct bt_work_q *queue,
		  struct bt_work *work)
{
	__ASSERT_NO_MSG(work != NULL);
	__ASSERT_NO_MSG(work->handler != NULL);

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));

	int ret = submit_to_queue_locked(work, &queue);

	/* No need to reschedule if the submission failed. */
	if (ret > 0) {
		bt_base_require(osi_mutex_unlock(&work_lock));
		bt_base_require(osi_thread_yield());
	} else {
		bt_base_require(osi_mutex_unlock(&work_lock));
	}

	return ret;
}

int bt_work_submit_to_queue(struct bt_work_q *queue,
			    struct bt_work *work)
{

	int ret = work_submit_to_queue(queue, work);

	return ret;
}

int bt_work_submit(struct bt_work *work)
{

	int ret = work_submit_to_queue(&bt_sys_work_q, work);

	return ret;
}

/* Flush the work item if necessary.
 *
 * Flushing is necessary only if the work is either queued or running.
 *
 * Invoked with work lock held.
 * Sleeps.
 *
 * @param work the work item that is to be flushed
 * @param flusher state used to synchronize the flush
 *
 * @retval true if work is queued or running.  If this happens the
 * caller must take the flusher semaphore after releasing the lock.
 *
 * @retval false otherwise.  No wait required.
 */
static bool work_flush_locked(struct bt_work *work,
			      struct bt_work_flusher *flusher)
{
	bool need_flush = (flags_get(&work->flags)
			   & (BT_WORK_QUEUED | BT_WORK_RUNNING)) != 0U;

	if (need_flush) {
		struct bt_work_q *queue = work->queue;

		__ASSERT_NO_MSG(queue != NULL);

		queue_flusher_locked(queue, work, flusher);
		notify_queue_locked(queue);
	}

	return need_flush;
}

bool bt_work_flush(struct bt_work *work,
		  struct bt_work_sync *sync)
{
	struct bt_work *item = work;

	__ASSERT_NO_MSG(work != NULL);
	__ASSERT_NO_MSG(!flag_test(&work->flags, BT_WORK_DELAYABLE_BIT));
	__ASSERT_NO_MSG(sync != NULL);

	struct bt_work_flusher *flusher = &sync->flusher;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	bt_base_require(!item->queue || !item->queue->started ||
		!osi_thread_is_current(&item->queue->thread) ? 0 : -EDEADLK);

	bool need_flush = work_flush_locked(work, flusher);

	bt_base_require(osi_mutex_unlock(&work_lock));

	/* If necessary wait until the flusher item completes */
	if (need_flush) {

		wait_completion(&flusher->sem);
	}

	return need_flush;
}

/* Execute the non-waiting steps necessary to cancel a work item.
 *
 * Invoked with work lock held.
 *
 * @param work the work item to be canceled.
 *
 * @retval true if we need to wait for the work item to finish canceling
 * @retval false if the work item is idle
 *
 * @return Work busy flags captured under lock
 */
static int cancel_async_locked(struct bt_work *work)
{
	/* If we haven't already started canceling, do it now. */
	if (!flag_test(&work->flags, BT_WORK_CANCELING_BIT)) {
		/* Remove it from the queue, if it's queued. */
		queue_remove_locked(work->queue, work);
	}

	/* If it's still busy after it's been dequeued, then flag it
	 * as canceling.
	 */
	int ret = work_busy_get_locked(work);

	if (ret != 0) {
		flag_set(&work->flags, BT_WORK_CANCELING_BIT);
		ret = work_busy_get_locked(work);
	}

	return ret;
}

/* Complete cancellation necessary, release work lock, and wait if
 * necessary.
 *
 * Invoked with work lock held.
 * The caller waits through its own semaphore if needed.
 *
 * @param work work that is being canceled
 * @param canceller state used to synchronize the cancellation
 *
 * @retval true if and only if the work was still active on entry.  The caller
 * must wait on the canceller semaphore after releasing the lock.
 *
 * @retval false if work was idle on entry.  The caller need not wait.
 */
static bool cancel_sync_locked(struct bt_work *work,
			       struct bt_work_canceller *canceller)
{
	bool ret = flag_test(&work->flags, BT_WORK_CANCELING_BIT);

	/* If something's still running then we have to wait for
	 * completion, which is indicated when finish_cancel() gets
	 * invoked.
	 */
	if (ret) {
		init_work_cancel(canceller, work);
	}

	return ret;
}

int bt_work_cancel(struct bt_work *work)
{
	__ASSERT_NO_MSG(work != NULL);
	__ASSERT_NO_MSG(!flag_test(&work->flags, BT_WORK_DELAYABLE_BIT));

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	int ret = cancel_async_locked(work);

	bt_base_require(osi_mutex_unlock(&work_lock));

	return ret;
}

bool bt_work_cancel_sync(struct bt_work *work,
			struct bt_work_sync *sync)
{
	struct bt_work *item = work;

	__ASSERT_NO_MSG(work != NULL);
	__ASSERT_NO_MSG(sync != NULL);
	__ASSERT_NO_MSG(!flag_test(&work->flags, BT_WORK_DELAYABLE_BIT));


	struct bt_work_canceller *canceller = &sync->canceller;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	bt_base_require(!item->queue || !item->queue->started ||
		!osi_thread_is_current(&item->queue->thread) ? 0 : -EDEADLK);
	bool pending = (work_busy_get_locked(work) != 0U);
	bool need_wait = false;

	if (pending) {
		(void)cancel_async_locked(work);
		need_wait = cancel_sync_locked(work, canceller);
	}

	bt_base_require(osi_mutex_unlock(&work_lock));

	if (need_wait) {

		wait_completion(&canceller->sem);
	}

	return pending;
}

/* Loop executed by a work queue thread.
 *
 * @param workq_ptr pointer to the work queue structure
 */
static void work_queue_main(void *workq_ptr)
{

	struct bt_work_q *queue = (struct bt_work_q *)workq_ptr;

	while (true) {
		sys_snode_t *node;
		struct bt_work *work = NULL;
		bt_work_handler_t handler = NULL;

		bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
		bool yield;

		/* Check for and prepare any new work. */
		node = sys_slist_get(&queue->pending);
		if (node != NULL) {
			/* Mark that there's some work active that's
			 * not on the pending list.
			 */
			flag_set(&queue->flags, BT_WORK_QUEUE_BUSY_BIT);
			work = CONTAINER_OF(node, struct bt_work, node);
			flag_set(&work->flags, BT_WORK_RUNNING_BIT);
			flag_clear(&work->flags, BT_WORK_QUEUED_BIT);

			/* Static code analysis tool can raise a false-positive violation
			 * in the line below that 'work' is checked for null after being
			 * dereferenced.
			 *
			 * The work is figured out by CONTAINER_OF, as a container
			 * of type struct bt_work that contains the node.
			 * The only way for it to be NULL is if node would be a member
			 * of struct bt_work object that has been placed at address NULL,
			 * which should never happen, even line 'if (work != NULL)'
			 * ensures that.
			 * This means that if node is not NULL, then work will not be NULL.
			 */
			handler = work->handler;
		} else if (flag_test_and_clear(&queue->flags,
					       BT_WORK_QUEUE_DRAIN_BIT)) {
			/* Not busy and draining: move threads waiting for
			 * drain to ready state. The mutex serializes this notification
			 * with the drain callers; each caller owns a semaphore.
			 *
			 * We don't touch BT_WORK_QUEUE_PLUGGED, so getting
			 * here doesn't mean that the queue will allow new
			 * submissions.
			 */
			drain_wake_all_locked(queue);
		} else if (flag_test(&queue->flags, BT_WORK_QUEUE_STOP_BIT)) {
			/* User has requested that the queue stop. Clear the status flags and exit.
			 */
			flags_set(&queue->flags, 0);
			queue->started = false;
			bt_base_require(osi_mutex_unlock(&work_lock));
			return;
		} else {
			/* No work is available and no queue state requires
			 * special handling.
			 */
			;
		}

		if (work == NULL) {
			/* Nothing's had a chance to add work since we took
			 * the lock, and we didn't find work nor got asked to
			 * stop.  Just go to sleep: when something happens the
			 * work thread will be woken and we can check again.
			 */

			bt_base_require(osi_mutex_unlock(&work_lock));
			bt_base_require(osi_sem_take(&queue->notifyq, OS_TIMEOUT_FOREVER));
			continue;
		}

		bt_base_require(osi_mutex_unlock(&work_lock));

		__ASSERT_NO_MSG(handler != NULL);
		handler(work);

		/* Mark the work item as no longer running and deal
		 * with any cancellation and flushing issued while it
		 * was running.  Clear the BUSY flag and optionally
		 * yield to prevent starving other threads.
		 */
		bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));

		flag_clear(&work->flags, BT_WORK_RUNNING_BIT);
		if (flag_test(&work->flags, BT_WORK_FLUSHING_BIT)) {
			finalize_flush_locked(work);
		}
		if (flag_test(&work->flags, BT_WORK_CANCELING_BIT)) {
			finalize_cancel_locked(work);
		}

		flag_clear(&queue->flags, BT_WORK_QUEUE_BUSY_BIT);
		yield = (!flag_test(&queue->flags, BT_WORK_QUEUE_NO_YIELD_BIT) &&
			 !sys_slist_is_empty(&queue->pending));
		bt_base_require(osi_mutex_unlock(&work_lock));

		/* Optionally yield to prevent the work queue from
		 * starving other threads.
		 */
		if (yield) {
			bt_base_require(osi_thread_yield());
		}
	}
}

static int timeout_service_start(void)
{
	int err;

	if (timeout_started) {
		return 0;
	}
	err = osi_sem_init(&timeout_notify, 0, 1);
	if (err) {
		return err;
	}
	sys_slist_init(&timeouts);
	timeout_stopping = false;
	err = osi_thread_create(&timeout_thread, timeout_main, NULL, "BT timeout",
				CONFIG_EMBT_TIMEOUT_THREAD_PRIO,
				CONFIG_EMBT_TIMEOUT_THREAD_STACK_SIZE);
	if (err) {
		bt_base_require(osi_sem_deinit(&timeout_notify));
		return err;
	}
	timeout_started = true;
	return 0;
}

int bt_work_queue_init(struct bt_work_q *queue)
{
	int err;

	if (!queue) {
		return -EINVAL;
	}
	if (queue->initialized) {
		return queue->started ? -EALREADY : 0;
	}
	/* The lifecycle owner serializes initialization and final teardown. */
	if (!work_lock_initialized) {
		err = osi_mutex_init(&work_lock, OS_MUTEX_NORMAL);
		if (err) {
			return err;
		}
		work_lock_initialized = true;
	}
	err = osi_sem_init(&queue->notifyq, 0, 1);
	if (err) {
		return err;
	}
	err = timeout_service_start();
	if (err) {
		bt_base_require(osi_sem_deinit(&queue->notifyq));
		return err;
	}
	sys_slist_init(&queue->pending);
	sys_slist_init(&queue->drainers);
	queue->flags = 0;
	queue->started = false;
	queue->joinable = false;
	queue->initialized = true;
	initialized_queues++;
	return 0;
}

int bt_work_queue_start(struct bt_work_q *queue, size_t stack_size, int priority,
			const struct bt_work_queue_config *config)
{
	int err;

	if (!queue || !queue->initialized) {
		return -EINVAL;
	}
	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	if (queue->started || queue->joinable) {
		bt_base_require(osi_mutex_unlock(&work_lock));
		return -EALREADY;
	}
	queue->flags = BT_WORK_QUEUE_STARTED;
	if (config && config->no_yield) {
		queue->flags |= BT_WORK_QUEUE_NO_YIELD;
	}
	queue->started = true;
	err = osi_thread_create(&queue->thread, work_queue_main, queue,
				config ? config->name : NULL, priority, stack_size);
	if (err) {
		queue->flags = 0;
		queue->started = false;
	} else {
		queue->joinable = true;
	}
	bt_base_require(osi_mutex_unlock(&work_lock));
	return err;
}

int bt_work_queue_deinit(struct bt_work_q *queue)
{
	sys_snode_t *node;
	int err;
	bool stop_timeout;

	if (!queue || !queue->initialized) {
		return 0;
	}
	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	if (queue->started || queue->joinable) {
		bt_base_require(osi_mutex_unlock(&work_lock));
		return -EBUSY;
	}
	SYS_SLIST_FOR_EACH_NODE(&timeouts, node) {
		struct bt_work_timeout *record = CONTAINER_OF(node, struct bt_work_timeout, node);
		struct bt_work_delayable *work = CONTAINER_OF(record, struct bt_work_delayable, timeout);

		if (work->queue == queue) {
			bt_base_require(osi_mutex_unlock(&work_lock));
			return -EBUSY;
		}
	}
	stop_timeout = initialized_queues == 1;
	if (stop_timeout) {
		timeout_stopping = true;
		bt_base_require(osi_sem_give(&timeout_notify));
	}
	bt_base_require(osi_mutex_unlock(&work_lock));
	if (stop_timeout) {
		err = osi_thread_join(&timeout_thread);
		if (err) {
			return err;
		}
		bt_base_require(osi_sem_deinit(&timeout_notify));
		timeout_started = false;
	}
	err = osi_sem_deinit(&queue->notifyq);
	if (!err) {
		queue->initialized = false;
		initialized_queues--;
	}
	return err;
}

os_thread_t *bt_work_queue_thread_get(struct bt_work_q *queue)
{
	bt_base_require(queue && queue->started ? 0 : -EINVAL);
	return &queue->thread;
}

int bt_work_queue_drain(struct bt_work_q *queue,
		       bool plug)
{
	__ASSERT_NO_MSG(queue);


	if (!queue || !queue->initialized || !queue->started) {
		return -ENODEV;
	}
	if (osi_thread_is_current(&queue->thread)) {
		return -EDEADLK;
	}
	int ret = 0;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));

	if (((flags_get(&queue->flags)
	      & (BT_WORK_QUEUE_BUSY | BT_WORK_QUEUE_DRAIN)) != 0U)
	    || plug
	    || !sys_slist_is_empty(&queue->pending)) {
		flag_set(&queue->flags, BT_WORK_QUEUE_DRAIN_BIT);
		if (plug) {
			flag_set(&queue->flags, BT_WORK_QUEUE_PLUGGED_BIT);
		}

		notify_queue_locked(queue);
		struct drain_waiter waiter;

		bt_base_require(osi_sem_init(&waiter.sem, 0, 1));
		sys_slist_append(&queue->drainers, &waiter.node);
		bt_base_require(osi_mutex_unlock(&work_lock));
		wait_completion(&waiter.sem);
	} else {
		bt_base_require(osi_mutex_unlock(&work_lock));
	}

	return ret;
}

int bt_work_queue_unplug(struct bt_work_q *queue)
{
	__ASSERT_NO_MSG(queue);

	int ret = -EALREADY;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));

	if (flag_test_and_clear(&queue->flags, BT_WORK_QUEUE_PLUGGED_BIT)) {
		ret = 0;
	}

	bt_base_require(osi_mutex_unlock(&work_lock));

	return ret;
}

int bt_work_queue_stop(struct bt_work_q *queue)
{
	int err;

	if (!queue || !queue->initialized) {
		return -EINVAL;
	}
	if (queue->joinable && osi_thread_is_current(&queue->thread)) {
		return -EDEADLK;
	}
	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	if (!flag_test(&queue->flags, BT_WORK_QUEUE_STARTED_BIT)) {
		bt_base_require(osi_mutex_unlock(&work_lock));
		return -EALREADY;
	}
	if (!flag_test(&queue->flags, BT_WORK_QUEUE_PLUGGED_BIT)) {
		bt_base_require(osi_mutex_unlock(&work_lock));
		return -EBUSY;
	}
	flag_set(&queue->flags, BT_WORK_QUEUE_STOP_BIT);
	notify_queue_locked(queue);
	bt_base_require(osi_mutex_unlock(&work_lock));
	/* OSI joins deliberately have no timeout. */
	err = osi_thread_join(&queue->thread);
	if (!err) {
		queue->joinable = false;
	}
	return err;
}

/* Timeout handler for delayable work.
 *
 * Invoked by timeout infrastructure.
 * Called with the work lock held for the entire timeout callback.
 * Conditionally submits work and notifies its queue.
 */
static void work_timeout_locked(struct bt_work_timeout *to)
{
	struct bt_work_delayable *dw
		= CONTAINER_OF(to, struct bt_work_delayable, timeout);
	struct bt_work *wp = &dw->work;
	struct bt_work_q *queue = NULL;

	/* BT_WORK_DELAYED_BIT is the wake-ownership flag: whichever side
	 * (this handler or unschedule_locked()) clears it via
	 * flag_test_and_clear() first owns the outcome. If unschedule_locked
	 * got here first, the bit is clear and we bail.
	 */
	if (flag_test_and_clear(&wp->flags, BT_WORK_DELAYED_BIT)) {
		queue = dw->queue;
		(void)submit_to_queue_locked(wp, &queue);
	}
}

void bt_work_init_delayable(struct bt_work_delayable *dwork,
			    bt_work_handler_t handler)
{
	__ASSERT_NO_MSG(dwork != NULL);
	__ASSERT_NO_MSG(handler != NULL);

	*dwork = (struct bt_work_delayable)BT_WORK_DELAYABLE_INITIALIZER(handler);

	dwork->timeout = (struct bt_work_timeout){0};
}

static inline int work_delayable_busy_get_locked(const struct bt_work_delayable *dwork)
{
	return flags_get(&dwork->work.flags) & BT_WORK_MASK;
}

int bt_work_delayable_busy_get(const struct bt_work_delayable *dwork)
{
	__ASSERT_NO_MSG(dwork != NULL);

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	int ret = work_delayable_busy_get_locked(dwork);

	bt_base_require(osi_mutex_unlock(&work_lock));
	return ret;
}

/* Attempt to schedule a work item for future (maybe immediate)
 * submission.
 *
 * Invoked with work lock held.
 *
 * See also submit_to_queue_locked(), which implements this for a no-wait
 * delay.
 *
 * Invoked with work lock held.
 *
 * @param queuep pointer to a pointer to a queue.  On input this
 * should dereference to the proposed queue (which may be null); after
 * completion it will be null if the work was not submitted or if
 * submitted will reference the queue it was submitted to.  That may
 * or may not be the queue provided on input.
 *
 * @param dwork the delayed work structure
 *
 * @param delay the delay to use before scheduling.
 *
 * @retval from submit_to_queue_locked() if delay is BT_NO_WAIT; otherwise
 * @retval 1 to indicate successfully scheduled.
 */
static int schedule_for_queue_locked(struct bt_work_q **queuep,
				     struct bt_work_delayable *dwork,
				     bt_timeout_t delay)
{
	int ret = 1;
	struct bt_work *work = &dwork->work;

	if (BT_TIMEOUT_EQ(delay, BT_NO_WAIT)) {
		return submit_to_queue_locked(work, queuep);
	}

	flag_set(&work->flags, BT_WORK_DELAYED_BIT);
	dwork->queue = *queuep;

	/* Add timeout */
	timeout_add_locked(&dwork->timeout, delay);

	return ret;
}

/* Unschedule delayable work.
 *
 * If the work is delayed, cancel the timeout and clear the delayed
 * flag.
 *
 * Invoked with work lock held.
 *
 * @param dwork pointer to delayable work structure.
 *
 * @return true if and only if work had been delayed so the timeout
 * was cancelled.
 */
static inline bool unschedule_locked(struct bt_work_delayable *dwork)
{
	if (!flag_test_and_clear(&dwork->work.flags, BT_WORK_DELAYED_BIT)) {
		return false;
	}
	/* The dispatcher invokes the complete timeout callback under work_lock.
	 * Acquiring it therefore also drains any callback already in progress.
	 */
	timeout_abort_locked(&dwork->timeout);
	return true;
}

/* Full cancellation of a delayable work item.
 *
 * Unschedules the delayed part then delegates to standard work
 * cancellation.
 *
 * Invoked with work lock held.
 *
 * @param dwork delayable work item
 *
 * @return bt_work_busy_get() flags
 */
static int cancel_delayable_async_locked(struct bt_work_delayable *dwork)
{
	(void)unschedule_locked(dwork);

	return cancel_async_locked(&dwork->work);
}

int bt_work_schedule_for_queue(struct bt_work_q *queue, struct bt_work_delayable *dwork,
			      bt_timeout_t delay)
{
	if (delay < BT_FOREVER) {
		return -EINVAL;
	}

	__ASSERT_NO_MSG(queue != NULL);
	__ASSERT_NO_MSG(dwork != NULL);

	struct bt_work *work = &dwork->work;
	int ret = 0;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));

	/* Schedule the work item if it's idle or running. */
	if ((work_busy_get_locked(work) & ~BT_WORK_RUNNING) == 0U) {
		ret = schedule_for_queue_locked(&queue, dwork, delay);
	}

	bt_base_require(osi_mutex_unlock(&work_lock));

	return ret;
}

int bt_work_schedule(struct bt_work_delayable *dwork, bt_timeout_t delay)
{

	int ret = bt_work_schedule_for_queue(&bt_sys_work_q, dwork, delay);

	return ret;
}

int bt_work_reschedule_for_queue(struct bt_work_q *queue, struct bt_work_delayable *dwork,
				bt_timeout_t delay)
{
	if (delay < BT_FOREVER) {
		return -EINVAL;
	}

	__ASSERT_NO_MSG(queue != NULL);
	__ASSERT_NO_MSG(dwork != NULL);

	int ret;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));

	/* Remove any active scheduling. */
	(void)unschedule_locked(dwork);

	/* Schedule the work item with the new parameters. */
	ret = schedule_for_queue_locked(&queue, dwork, delay);

	bt_base_require(osi_mutex_unlock(&work_lock));

	return ret;
}

int bt_work_reschedule(struct bt_work_delayable *dwork, bt_timeout_t delay)
{

	int ret = bt_work_reschedule_for_queue(&bt_sys_work_q, dwork, delay);

	return ret;
}

int bt_work_cancel_delayable(struct bt_work_delayable *dwork)
{
	__ASSERT_NO_MSG(dwork != NULL);

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	int ret = cancel_delayable_async_locked(dwork);

	bt_base_require(osi_mutex_unlock(&work_lock));

	return ret;
}

bool bt_work_cancel_delayable_sync(struct bt_work_delayable *dwork,
				  struct bt_work_sync *sync)
{
	struct bt_work *item = &dwork->work;

	__ASSERT_NO_MSG(dwork != NULL);
	__ASSERT_NO_MSG(sync != NULL);


	struct bt_work_canceller *canceller = &sync->canceller;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	bt_base_require(!item->queue || !item->queue->started ||
		!osi_thread_is_current(&item->queue->thread) ? 0 : -EDEADLK);
	bool pending = (work_delayable_busy_get_locked(dwork) != 0U);
	bool need_wait = false;

	if (pending) {
		(void)cancel_delayable_async_locked(dwork);
		need_wait = cancel_sync_locked(&dwork->work, canceller);
	}

	bt_base_require(osi_mutex_unlock(&work_lock));

	if (need_wait) {
		wait_completion(&canceller->sem);
	}

	return pending;
}

bool bt_work_flush_delayable(struct bt_work_delayable *dwork,
			    struct bt_work_sync *sync)
{
	struct bt_work *item = &dwork->work;

	__ASSERT_NO_MSG(dwork != NULL);
	__ASSERT_NO_MSG(sync != NULL);


	struct bt_work *work = &dwork->work;
	struct bt_work_flusher *flusher = &sync->flusher;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	bt_base_require(!item->queue || !item->queue->started ||
		!osi_thread_is_current(&item->queue->thread) ? 0 : -EDEADLK);

	/* If it's idle release the lock and return immediately. */
	if (work_busy_get_locked(work) == 0U) {
		bt_base_require(osi_mutex_unlock(&work_lock));

		return false;
	}

	/* If unscheduling did something then submit it.  Ignore a
	 * failed submission (e.g. when cancelling).
	 */
	if (unschedule_locked(dwork)) {
		struct bt_work_q *queue = dwork->queue;

		(void)submit_to_queue_locked(work, &queue);
	}

	/* Wait for it to finish */
	bool need_flush = work_flush_locked(work, flusher);

	bt_base_require(osi_mutex_unlock(&work_lock));

	/* If necessary wait until the flusher item completes */
	if (need_flush) {
		wait_completion(&flusher->sem);
	}

	return need_flush;
}

static void timeout_main(void *arg)
{
	(void)arg;
	for (;;) {
		os_timeout_t wait = OS_TIMEOUT_FOREVER;
		sys_snode_t *node;

		bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
		if (timeout_stopping) {
			bt_base_require(osi_mutex_unlock(&work_lock));
			return;
		}
		while ((node = sys_slist_peek_head(&timeouts)) != NULL) {
			struct bt_work_timeout *record = CONTAINER_OF(node, struct bt_work_timeout, node);

			if (record->deadline_ms == INT64_MAX) {
				break;
			}
			wait = MAX(INT64_C(0), record->deadline_ms - bt_uptime_get_ms());
			if (wait > 0) {
				break;
			}
			sys_slist_get(&timeouts);
			record->scheduled = false;
			work_timeout_locked(record);
			wait = OS_TIMEOUT_FOREVER;
		}
		bt_base_require(osi_mutex_unlock(&work_lock));
		int err = osi_sem_take(&timeout_notify, wait);

		if (err != -ETIMEDOUT) {
			bt_base_require(err);
		}
	}
}

int64_t bt_work_delayable_remaining_get(const struct bt_work_delayable *work)
{
	int64_t remaining = 0;

	bt_base_require(osi_mutex_lock(&work_lock, OS_TIMEOUT_FOREVER));
	if (work->timeout.scheduled) {
		remaining = work->timeout.deadline_ms == INT64_MAX ? INT64_MAX :
			MAX(INT64_C(0), work->timeout.deadline_ms - bt_uptime_get_ms());
	}
	bt_base_require(osi_mutex_unlock(&work_lock));
	return remaining;
}
