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
 * @brief Internal OSI raw console output and fatal termination.
 */

#ifndef EMBT_OSI_IO_H_
#define EMBT_OSI_IO_H_

#include <osi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write raw bytes to the POSIX stderr console.
 *
 * @param[in] data Readable byte buffer; NULL is accepted only when length is zero.
 * @param[in] length Number of bytes to write.
 * @param[out] written Required byte-count result; must not be NULL. It is set to zero before work,
 *   and may contain a partial count on failure.
 *
 * @retval 0 All requested bytes were written.
 * @return A negative error code on failure, possibly after a partial write.
 *
 * @par Context
 * Thread context. This operation may block.
 *
 * @par Concurrency
 * Concurrent writes do not guarantee message-level atomicity.
 *
 * @note This API does not format text, select severity or implement an asynchronous logger.
 */
int osi_console_write(const void *data, size_t length, size_t *written);

/**
 * @brief Terminate execution after a fatal error.
 *
 * @param[in] message Optional diagnostic message; may be NULL.
 *
 * @note The POSIX backend writes an optional diagnostic and terminates with abort() (SIGABRT).
 *   Output errors do not change its non-returning behavior.
 */
OS_NORETURN void osi_fatal(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_IO_H_ */
