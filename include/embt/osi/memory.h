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
 * @brief Internal OSI system heap allocation and release.
 *
 * These APIs provide a heap source, not buffer or pool allocation policy.
 * Allocation failures return NULL; errno is not part of the allocation API contract.
 */

#ifndef EMBT_OSI_MEMORY_H_
#define EMBT_OSI_MEMORY_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate memory from the OSI system heap.
 *
 * @param[in] size Number of bytes to allocate.
 *
 * @return Allocated memory, or NULL for a zero size or allocation failure.
 *
 * @par Ownership
 * Release the allocation with osi_free(). This API does not allocate net_buf or pool objects.
 */
void *osi_malloc(size_t size);

/**
 * @brief Allocate zero-filled elements from the OSI system heap.
 *
 * @param[in] count Number of elements.
 * @param[in] size Size of each element in bytes.
 *
 * @return Zero-filled memory, or NULL for a zero size, multiplication overflow or allocation
 *   failure.
 *
 * @par Ownership
 * Release the allocation with osi_free().
 */
void *osi_calloc(size_t count, size_t size);

/**
 * @brief Allocate aligned memory from the OSI system heap.
 *
 * @param[in] alignment Power-of-two byte alignment, at least sizeof(void *).
 * @param[in] size Number of bytes to allocate; it need not be a multiple of alignment.
 *
 * @return Aligned memory, or NULL for an invalid request, zero size or allocation failure.
 *
 * @par Ownership
 * Release the allocation with osi_free().
 */
void *osi_aligned_alloc(size_t alignment, size_t size);

/**
 * @brief Release an allocation from this OSI system heap.
 *
 * @param[in] pointer Allocation returned by this backend's allocation APIs, or NULL.
 *
 * @pre A non-NULL pointer belongs to this backend and has not already been released.
 * @note NULL is accepted; this function does not release buffer or pool objects.
 */
void osi_free(void *pointer);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_MEMORY_H_ */
