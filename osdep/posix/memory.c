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
 * @brief POSIX system heap source with checked allocation requests.
 */

#include <osi/memory.h>
#include <stdint.h>
#include <stdlib.h>

void *osi_malloc(size_t size)
{
	return size ? malloc(size) : NULL;
}

void *osi_calloc(size_t count, size_t size)
{
	if (!count || !size || count > SIZE_MAX / size) {
		return NULL;
	}
	return calloc(count, size);
}

void *osi_aligned_alloc(size_t alignment, size_t size)
{
	void *memory = NULL;

	if (!size || alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
		return NULL;
	}
	return posix_memalign(&memory, alignment, size) == 0 ? memory : NULL;
}

void osi_free(void *pointer)
{
	free(pointer);
}
