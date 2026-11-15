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
 * @brief Native POSIX types used by the internal OSI interface.
 *
 * Native object fields belong to the backend. Initialized synchronization objects have stable
 *   addresses and must not be copied or overwritten.
 */

#ifndef EMBT_OSI_POSIX_NATIVE_TYPES_H_
#define EMBT_OSI_POSIX_NATIVE_TYPES_H_

#include <osi/common.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

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
 * @brief Native thread identity value.
 *
 * Use osi_thread_id_equal() to compare identities and keep their native lifetime valid.
 */
typedef pthread_t os_tid_t;

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

/**
 * @brief Native thread handle without an additional control state machine.
 *
 * Creation starts execution. The caller owns the join responsibility and serializes lifecycle
 *   operations.
 */
typedef struct os_thread {
	pthread_t native; /**< Native joinable thread identity. */
} os_thread_t;

/**
 * @brief Native OSI timer storage passed to expiration callbacks.
 */
typedef struct os_timer os_timer_t;

/**
 * @brief Publish a native timer expiration notification.
 *
 * @param[in] timer Timer that produced the notification; must not be NULL.
 * @param[in,out] argument Borrowed caller context; may be NULL if the callback accepts it.
 *
 * @par Context
 * Native POSIX notification thread. Invocations may overlap; synchronize shared data and do not
 *   call timer APIs.
 *
 * @par Lifetime
 * Timer storage and context outlive every possible notification, including callbacks dispatched
 *   before native timer deletion.
 */
typedef void (*os_timer_cb_t)(os_timer_t *timer, void *argument);

/**
 * @brief Native timer handle and notification data.
 *
 * Native deletion does not drain callbacks. Retain storage and do not change callback fields while
 *   any notification may still run.
 */
struct os_timer {
	timer_t native; /**< Native scheduling handle, valid until deletion. */
	os_timer_cb_t callback; /**< Notification routine retained for callback lifetime. */
	void *argument; /**< Borrowed notification context retained for callback lifetime. */
};

#endif /* EMBT_OSI_POSIX_NATIVE_TYPES_H_ */
