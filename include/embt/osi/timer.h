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
 * @brief Internal OSI one-shot native timer operations.
 */

#ifndef EMBT_OSI_TIMER_H_
#define EMBT_OSI_TIMER_H_

#include <embt/osi/common.h>
#include <time.h>

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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a one-shot native timer without arming it.
 *
 * @param[out] timer Storage for the timer; must not be NULL.
 * @param[in] callback Expiration notification callback; must not be NULL.
 * @param[in,out] argument Borrowed callback context; may be NULL if the callback accepts it.
 *
 * @retval 0 The native timer was created and remains disarmed.
 * @return A negative error code on failure.
 *
 * @pre The caller serializes timer lifecycle operations.
 *
 * @par Callback
 * CLOCK_MONOTONIC scheduling uses native SIGEV_THREAD notification. Invocations can overlap,
 *   including for the same timer. The callback only publishes a short notification and does not
 *   call timer APIs.
 *
 * @par Lifetime
 * Storage, callback fields and context outlive every possible notification, including notifications
 *   dispatched before native deletion.
 */
int osi_timer_create(os_timer_t *timer, os_timer_cb_t callback, void *argument);

/**
 * @brief Arm or rearm a one-shot native timer.
 *
 * @param[in,out] timer Valid created native timer; must not be NULL.
 * @param[in] timeout_ms Positive relative expiration delay in milliseconds.
 *
 * @retval 0 The native expiration schedule was set.
 * @retval -EINVAL Zero, OS_TIMEOUT_FOREVER or another negative delay was supplied.
 * @return A negative error code for other failures.
 *
 * @pre The caller serializes timer API operations and does not call from its callback.
 * @note Rearming does not retract a notification already dispatched by the OS.
 */
int osi_timer_start(os_timer_t *timer, os_timeout_t timeout_ms);

/**
 * @brief Disarm a native timer's future expiration schedule.
 *
 * @param[in,out] timer Valid created native timer; must not be NULL.
 *
 * @retval 0 The native expiration schedule was disarmed.
 * @return A negative error code on failure.
 *
 * @pre The caller serializes timer API operations and does not call from its callback.
 * @note Already dispatched callbacks may still run; stop is not a callback completion barrier.
 */
int osi_timer_stop(os_timer_t *timer);

/**
 * @brief Delete native timer scheduling resources.
 *
 * @param[in,out] timer Valid created native timer; must not be NULL.
 *
 * @retval 0 Native scheduling resources were deleted.
 * @return A negative error code on failure.
 *
 * @pre The caller serializes lifecycle operations and prevents callback-side timer API use.
 *
 * @par Lifetime
 * Deletion does not wait for callbacks. Retain timer storage, callback fields and context for all
 *   possible notifications.
 *
 * @warning Do not clear, reuse or free timer/context storage based on this call returning.
 */
int osi_timer_delete(os_timer_t *timer);

/**
 * @brief Read a native timer's remaining schedule as a snapshot.
 *
 * @param[in] timer Valid created native timer; must not be NULL.
 * @param[out] milliseconds Remaining delay in milliseconds; must not be NULL. Errors leave it
 *   unchanged.
 *
 * @retval 0 The remaining delay was obtained.
 * @return A negative error code on failure.
 *
 * @pre The caller serializes timer API operations and does not call from its callback.
 * @note An inactive schedule is zero; a positive sub-millisecond remainder rounds up. Zero does not
 *   imply that callbacks have completed.
 */
int osi_timer_remaining_ms(os_timer_t *timer, uint64_t *milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_TIMER_H_ */
