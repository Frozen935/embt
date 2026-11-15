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
 * @brief Private POSIX deadline conversion used by OSI waits.
 */

#ifndef EMBT_OSI_POSIX_PRIVATE_H_
#define EMBT_OSI_POSIX_PRIVATE_H_
#include <osi/common.h>
#include <time.h>
/**
 * @brief Convert a finite interval into an absolute monotonic deadline.
 * @param[in] milliseconds Nonnegative relative interval in milliseconds.
 * @param[out] deadline Result, unchanged on failure; must not be NULL.
 * @return Zero on success or a negative error code.
 */
int osi_posix_deadline(os_timeout_t milliseconds, struct timespec *deadline);
/**
 * @brief Convert a nonnegative interval into a native relative timespec.
 * @param[in] milliseconds Nonnegative relative interval in milliseconds.
 * @param[out] interval Result, unchanged on failure; must not be NULL.
 * @return Zero on success or a negative error code.
 */
int osi_posix_interval(os_timeout_t milliseconds, struct timespec *interval);
#endif
