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
 * @brief Internal OSI monotonic time and checked timeout conversion.
 */

#ifndef EMBT_OSI_TIME_H_
#define EMBT_OSI_TIME_H_

#include <osi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read monotonic time from the backend-defined origin.
 *
 * @param[out] milliseconds Monotonic time in milliseconds; must not be NULL and is valid only on
 *   success.
 *
 * @retval 0 The timestamp was obtained.
 * @return A negative error code on failure.
 *
 * @note This timestamp is not UTC or wall-clock time.
 */
int osi_time_get_ms(uint64_t *milliseconds);

/**
 * @brief Sleep for a finite nonnegative interval.
 *
 * @param[in] milliseconds Relative delay in milliseconds; zero returns immediately.
 *
 * @retval 0 The requested finite wait completed.
 * @retval -EINVAL A negative value, including OS_TIMEOUT_FOREVER, was supplied.
 * @return A negative error code for other failures.
 *
 * @par Context
 * Thread context. Interruptions resume the remaining interval.
 *
 * @note A zero delay does not explicitly yield; use osi_thread_yield() for that operation.
 */
int osi_sleep_ms(os_timeout_t milliseconds);

/**
 * @brief Convert nonnegative microseconds to relative milliseconds.
 *
 * @param[in] microseconds Nonnegative interval in microseconds.
 * @param[out] milliseconds Converted timeout; must not be NULL. Errors leave it unchanged.
 *
 * @retval 0 The interval was converted.
 * @retval -EINVAL The interval was negative.
 * @return A negative error code for other failures.
 *
 * @note Positive intervals round up; a positive fraction of one millisecond becomes one.
 */
int osi_timeout_from_us(int64_t microseconds, os_timeout_t *milliseconds);

/**
 * @brief Convert nonnegative seconds to relative milliseconds.
 *
 * @param[in] seconds Nonnegative interval in seconds.
 * @param[out] milliseconds Converted timeout; must not be NULL. Errors leave it unchanged.
 *
 * @retval 0 The interval was converted.
 * @retval -EINVAL The interval was negative.
 * @retval -EOVERFLOW Multiplication would exceed the timeout representation.
 * @return A negative error code for other failures.
 */
int osi_timeout_from_seconds(int64_t seconds, os_timeout_t *milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_TIME_H_ */
