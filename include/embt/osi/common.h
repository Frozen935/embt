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
 * @brief Common OSI types and constants for internal stack components.
 *
 * Status functions return zero or a negative errno value; allocation failures return NULL.
 * OSI operations use thread context, without ISR or asynchronous signal-handler variants.
 */

#ifndef EMBT_OSI_COMMON_H_
#define EMBT_OSI_COMMON_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

/**
 * @brief Relative timeout in milliseconds.
 *
 * Zero requests no wait, minus one requests an indefinite wait, and positive values are finite.
 *   Values below minus one are invalid; conversions check representation overflow.
 */
typedef int64_t os_timeout_t;

/**
 * @brief Request a nonblocking attempt from an OSI wait operation.
 */
#define OS_TIMEOUT_NO_WAIT INT64_C(0)

/**
 * @brief Request an indefinite wait from an operation that supports it.
 */
#define OS_TIMEOUT_FOREVER INT64_C(-1)





#if defined(__cplusplus)
/**
 * @brief Mark a function that does not return to its caller.
 */
#define OS_NORETURN [[noreturn]]
#else
/**
 * @brief Mark a function that does not return to its caller.
 */
#define OS_NORETURN _Noreturn
#endif

#if defined(__cplusplus)
/**
 * @brief Initialize object storage without creating native resources.
 *
 * Use explicit init/create APIs before operating on a native object. Zero storage is not an
 *   object-validity marker.
 */
#define OS_OBJECT_STORAGE_INIT {}
#else
/**
 * @brief Initialize object storage without creating native resources.
 *
 * Use explicit init/create APIs before operating on a native object. Zero storage is not an
 *   object-validity marker.
 */
#define OS_OBJECT_STORAGE_INIT {0}
#endif

#endif /* EMBT_OSI_COMMON_H_ */
