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

#include <embt/osi/common.h>
#include <pthread.h>

/**
 * @brief Native thread handle used for managed threads and borrowed identities.
 *
 * Creation starts execution and gives the caller one join responsibility. A handle returned by
 *   osi_thread_self() only borrows the calling thread's identity. Copying a handle does not create
 *   a thread or another join responsibility. Compare valid handles with osi_thread_equal(); do not
 *   assume an integer representation or a portable invalid value.
 */
typedef pthread_t os_thread_t;

/**
 * @brief User routine executed by an OSI thread.
 *
 * @param[in,out] argument Borrowed caller context; may be NULL if the routine accepts it.
 *
 * @par Lifetime
 * The context remains valid until native termination is confirmed by join. Returning from the
 *   routine ends user execution.
 */
typedef void (*os_thread_entry_t)(void *argument);

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
 * @param[in] priority Original caller priority; Linux defaults, NuttX reference-minus-request.
 * @param[in] stack_size Requested byte count; Linux defaults, NuttX native stack attribute.
 *
 * @retval 0 The native thread was created and may already be running.
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
 * @note Linux intentionally does not apply priority or stack_size. This preserves protocol
 *   configuration values without claiming real-time scheduling or custom Linux stacks.
 *   The shared NuttX branch validates and applies the converted priority and byte-stack request.
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
 * @brief Borrow the calling thread's native handle.
 *
 * @return Borrowed handle identifying the calling thread.
 *
 * @note This does not create join ownership. Compare valid handles with osi_thread_equal(), not
 *   integer or pointer assumptions. Do not retain the handle beyond its native identity lifetime.
 */
os_thread_t osi_thread_self(void);

/**
 * @brief Check whether a native handle identifies the calling thread.
 *
 * @param[in] thread Valid managed or borrowed native handle; must not be NULL.
 *
 * @return true if the native identity matches the caller, otherwise false.
 */
bool osi_thread_is_current(const os_thread_t *thread);

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
 * @brief Compare two valid native thread handles.
 *
 * @param[in] first First managed or borrowed handle.
 * @param[in] second Second managed or borrowed handle.
 *
 * @return true if the handles refer to the same native thread, otherwise false.
 *
 * @pre Both native identities remain valid for the comparison.
 * @note Comparing or copying a handle does not transfer join ownership.
 */
bool osi_thread_equal(os_thread_t first, os_thread_t second);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_THREAD_H_ */
