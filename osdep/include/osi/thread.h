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
 * @brief Internal OSI thread lifecycle and identity operations.
 */

#ifndef EMBT_OSI_THREAD_H_
#define EMBT_OSI_THREAD_H_

#include <osi/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a native thread that can immediately enter its routine.
 *
 * @param[out] thread Storage for the native handle; must not be NULL.
 * @param[in] start_routine User entry routine; must not be NULL. Returning ends user execution.
 * @param[in,out] argument Borrowed entry context; may be NULL if the routine accepts it.
 * @param[in] name Optional diagnostic name, consumed during creation; may be NULL.
 * @param[in] priority Scheduling request. The current POSIX baseline accepts only zero.
 * @param[in] stack_size Native stack size in bytes; zero selects the system default.
 *
 * @retval 0 The native thread was created and may already be running.
 * @retval -ENOTSUP The requested priority is unsupported by the current POSIX baseline.
 * @return A negative error code for other creation failures.
 *
 * @pre Initialize all resources used by the routine before creating the thread.
 *
 * @par Lifetime
 * The handle and entry context remain valid until successful join. Failed creation releases
 *   temporary resources and leaves no live user thread.
 *
 * @note Name is a diagnostic hint. Use osi_thread_name_set() to obtain an explicit naming result; a
 *   post-create naming failure does not undo thread creation.
 */
int osi_thread_create(os_thread_t *thread, void (*start_routine)(void *), void *argument,
		      const char *name, int priority, size_t stack_size);

/**
 * @brief Preserve the OpenBlue start call as a POSIX compatibility operation.
 *
 * @param[in] thread Valid created, unjoined native handle; must not be NULL.
 *
 * @retval 0 The compatibility operation completed.
 *
 * @note POSIX create already starts the thread. This operation does not delay, restart or resume
 *   execution.
 */
int osi_thread_start(os_thread_t *thread);

/**
 * @brief Submit a native thread cancellation request.
 *
 * @param[in] thread Valid created, unjoined native handle; must not be NULL.
 *
 * @retval 0 The native cancellation request was submitted.
 * @return A negative error code on failure.
 *
 * @par Lifetime
 * The caller still joins the thread before releasing its entry context.
 *
 * @note Cancellation depends on the target's cancellation state and points. Success is not a
 *   termination guarantee; business resources require their own cleanup.
 */
int osi_thread_cancel(os_thread_t *thread);

/**
 * @brief Wait for native thread termination and release its joinable resources.
 *
 * @param[in,out] thread Valid created, unjoined native handle; must not be NULL.
 *
 * @retval 0 Native termination was confirmed and joinable resources were released.
 * @return A negative error code on failure.
 *
 * @pre Join once, serialize lifecycle operations and never join the calling thread itself.
 *
 * @par Context
 * Thread context. This operation blocks without a timeout.
 *
 * @par Lifetime
 * The entry context may be released after successful join, including after cancellation.
 */
int osi_thread_join(os_thread_t *thread);

/**
 * @brief Get the calling thread's native identity.
 *
 * @return Native identity of the calling thread.
 *
 * @note Compare identities with osi_thread_id_equal(), not integer or pointer assumptions.
 */
os_tid_t osi_thread_self(void);

/**
 * @brief Check whether a managed thread is the calling thread.
 *
 * @param[in] thread Valid created, unjoined native handle; must not be NULL.
 *
 * @return true if the native identity matches the caller, otherwise false.
 */
bool osi_thread_is_current(os_thread_t *thread);

/**
 * @brief Yield execution to the native scheduler.
 *
 * @retval 0 The scheduler yield operation completed.
 * @return A negative error code on failure.
 */
int osi_thread_yield(void);

/**
 * @brief Set a native thread's diagnostic name.
 *
 * @param[in] thread Valid created, unjoined native handle; must not be NULL.
 * @param[in] name Name meeting native naming limits; must not be NULL.
 *
 * @retval 0 The native name was set.
 * @return A negative error code on failure.
 *
 * @note A naming failure does not terminate or release the thread.
 */
int osi_thread_name_set(os_thread_t *thread, const char *name);

/**
 * @brief Compare two valid native thread identities.
 *
 * @param[in] first First native identity.
 * @param[in] second Second native identity.
 *
 * @return true if the identities refer to the same native thread, otherwise false.
 *
 * @pre Both identities remain valid for the comparison.
 */
bool osi_thread_id_equal(os_tid_t first, os_tid_t second);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_THREAD_H_ */
