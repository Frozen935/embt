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
 * @brief Internal OSI mutex, semaphore and condition-variable operations.
 */

#ifndef EMBT_OSI_SYNC_H_
#define EMBT_OSI_SYNC_H_

#include <embt/osi/common.h>
#include <pthread.h>
#include <semaphore.h>

/**
 * @brief Mutex ownership behavior selected during initialization.
 */
typedef enum {
	OS_MUTEX_NORMAL, /**< Non-recursive mutex ownership. */
	OS_MUTEX_RECURSIVE, /**< Recursive ownership with matching unlock operations. */
} os_mutex_kind_t;

/**
 * @brief Native POSIX mutex storage.
 *
 * Initialize and release through the OSI mutex APIs; storage contents are not a validity marker.
 */
typedef pthread_mutex_t os_mutex_t;

/**
 * @brief Native POSIX condition-variable storage.
 *
 * The caller coordinates predicate locking and ensures no waiters remain during deinitialization.
 */
typedef pthread_cond_t os_cond_t;

/**
 * @brief Native semaphore with serialized bounded give operations.
 *
 * The caller ensures no waiting takes or concurrent users remain during reset or deinitialization.
 */
typedef struct os_sem {
	sem_t native; /**< Native count and wait operation storage. */
	pthread_mutex_t give_lock; /**< Serialize the count-limit check and post. */
	uint32_t limit; /**< Maximum available count, fixed during initialization. */
} os_sem_t;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a normal or recursive mutex.
 *
 * @param[out] mutex Storage for the mutex; must not be NULL.
 * @param[in] kind Ownership behavior selected with OS_MUTEX_NORMAL or OS_MUTEX_RECURSIVE.
 *
 * @retval 0 The mutex was initialized.
 * @return A negative error code on failure.
 *
 * @pre The storage does not contain an active mutex and has no concurrent users.
 */
int osi_mutex_init(os_mutex_t *mutex, os_mutex_kind_t kind);

/**
 * @brief Release the native resources of a mutex.
 *
 * @param[in,out] mutex Initialized mutex to release; must not be NULL.
 *
 * @retval 0 The mutex resources were released.
 * @return A negative error code on failure.
 *
 * @pre The caller ensures the mutex is unlocked and has no waiters or concurrent users.
 * @note Storage contents do not indicate whether a native mutex is valid.
 */
int osi_mutex_deinit(os_mutex_t *mutex);

/**
 * @brief Acquire a mutex.
 *
 * @param[in,out] mutex Initialized mutex to acquire; must not be NULL.
 * @param[in] timeout_ms Relative timeout in milliseconds. OS_TIMEOUT_NO_WAIT requests a nonblocking
 *   attempt; OS_TIMEOUT_FOREVER waits indefinitely.
 *
 * @retval 0 The caller acquired the mutex.
 * @retval -EAGAIN The mutex was unavailable during a nonblocking attempt.
 * @retval -ETIMEDOUT A finite wait expired.
 * @return A negative error code for other failures.
 *
 * @pre A normal mutex must not be acquired recursively by its owner.
 * @post On success, the caller owns the mutex and must perform a matching unlock.
 *
 * @par Context
 * Thread context. The operation may block according to timeout_ms.
 */
int osi_mutex_lock(os_mutex_t *mutex, os_timeout_t timeout_ms);

/**
 * @brief Release one level of mutex ownership.
 *
 * @param[in,out] mutex Mutex owned by the caller; must not be NULL.
 *
 * @retval 0 One level of ownership was released.
 * @return A negative error code on failure.
 *
 * @pre The caller owns the mutex and matches every successful lock with one unlock.
 */
int osi_mutex_unlock(os_mutex_t *mutex);

/**
 * @brief Initialize a bounded native semaphore.
 *
 * @param[out] sem Storage for the semaphore; must not be NULL.
 * @param[in] initial_count Initial available count, not greater than limit.
 * @param[in] limit Positive maximum count, not greater than the native SEM_VALUE_MAX.
 *
 * @retval 0 The semaphore and give lock were initialized.
 * @return A negative error code on failure.
 *
 * @pre The storage has no active semaphore or concurrent users.
 */
int osi_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t limit);

/**
 * @brief Release a semaphore and its give lock.
 *
 * @param[in,out] sem Initialized semaphore to release; must not be NULL.
 *
 * @retval 0 The native resources were released.
 * @return A negative error code on failure.
 *
 * @pre The caller ensures there are no waiting takes or concurrent API users.
 */
int osi_sem_deinit(os_sem_t *sem);

/**
 * @brief Consume one available semaphore count.
 *
 * @param[in,out] sem Initialized semaphore; must not be NULL.
 * @param[in] timeout_ms Relative timeout in milliseconds. OS_TIMEOUT_NO_WAIT does not block;
 *   OS_TIMEOUT_FOREVER waits indefinitely.
 *
 * @retval 0 One count was consumed.
 * @retval -EAGAIN No count was available during a nonblocking attempt.
 * @retval -ETIMEDOUT A finite wait expired.
 * @return A negative error code for other failures.
 *
 * @par Context
 * Thread context. The native wait may block without holding the give lock.
 */
int osi_sem_take(os_sem_t *sem, os_timeout_t timeout_ms);

/**
 * @brief Give one semaphore count without exceeding its limit.
 *
 * @param[in,out] sem Initialized semaphore; must not be NULL.
 *
 * @retval 0 A count was posted, or the semaphore was already at its limit.
 * @return A negative error code on failure.
 *
 * @par Concurrency
 * Concurrent givers serialize the native value check and post through the give lock.
 */
int osi_sem_give(os_sem_t *sem);

/**
 * @brief Read a native semaphore count snapshot.
 *
 * @param[in] sem Initialized semaphore to inspect; must not be NULL.
 * @param[out] count Available count snapshot; must not be NULL and is valid only on success.
 *
 * @retval 0 The snapshot was obtained.
 * @return A negative error code on failure.
 *
 * @note The snapshot is for diagnostics; it does not reserve a count for a later take.
 */
int osi_sem_count_get(os_sem_t *sem, uint32_t *count);

/**
 * @brief Drain an externally quiescent semaphore to zero.
 *
 * @param[in,out] sem Initialized semaphore to drain; must not be NULL.
 *
 * @retval 0 The available count was cleared.
 * @return A negative error code on failure.
 *
 * @pre There are no waiting takes or concurrent API users.
 * @note This operation does not cancel waiters and is not equivalent to Zephyr k_sem_reset().
 */
int osi_sem_reset(os_sem_t *sem);

/**
 * @brief Initialize a condition variable using CLOCK_MONOTONIC.
 *
 * @param[out] cond Storage for the condition variable; must not be NULL.
 *
 * @retval 0 The condition variable was initialized.
 * @return A negative error code on failure.
 *
 * @pre The storage has no active condition variable or concurrent users.
 */
int osi_cond_init(os_cond_t *cond);

/**
 * @brief Release a native condition variable.
 *
 * @param[in,out] cond Initialized condition variable; must not be NULL.
 *
 * @retval 0 The native resources were released.
 * @return A negative error code on failure.
 *
 * @pre The caller ensures there are no waiters or concurrent API users.
 */
int osi_cond_deinit(os_cond_t *cond);

/**
 * @brief Wait for a predicate change while temporarily releasing its mutex.
 *
 * @param[in,out] cond Initialized condition variable; must not be NULL.
 * @param[in,out] mutex Caller-owned normal mutex protecting the predicate; must not be NULL.
 * @param[in] timeout_ms Relative timeout in milliseconds. OS_TIMEOUT_NO_WAIT does not wait;
 *   OS_TIMEOUT_FOREVER waits indefinitely.
 *
 * @retval 0 The native wait returned; the caller must recheck the predicate.
 * @retval -EAGAIN A nonblocking wait was requested; the mutex remains held.
 * @retval -ETIMEDOUT A finite wait expired.
 * @return A negative error code for other failures.
 *
 * @pre Active waiters use the same non-recursive mutex, already held by the caller.
 * @post After a blocking wait, the caller again holds the predicate mutex.
 *
 * @par Context
 * Thread context. Blocking waits atomically register and release the mutex.
 *
 * @note Spurious wakeups are possible; check the predicate in a loop.
 */
int osi_cond_wait(os_cond_t *cond, os_mutex_t *mutex, os_timeout_t timeout_ms);

/**
 * @brief Notify one condition-variable waiter.
 *
 * @param[in,out] cond Initialized condition variable; must not be NULL.
 *
 * @retval 0 The notification was issued.
 * @return A negative error code on failure.
 *
 * @pre Update the predicate and notify while holding its mutex.
 * @note Notifications do not accumulate credits or select a particular waiter.
 */
int osi_cond_signal(os_cond_t *cond);

/**
 * @brief Notify all current condition-variable waiters.
 *
 * @param[in,out] cond Initialized condition variable; must not be NULL.
 *
 * @retval 0 The notification was issued.
 * @return A negative error code on failure.
 *
 * @pre Update the predicate and notify while holding its mutex.
 * @note Each waiter must recheck the predicate; notifications do not accumulate credits.
 */
int osi_cond_broadcast(os_cond_t *cond);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_SYNC_H_ */
