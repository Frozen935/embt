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
 * @brief Native-endian reads from ordinary, potentially unaligned byte storage.
 */

#ifndef EMBT_SYS_UNALIGNED_H_
#define EMBT_SYS_UNALIGNED_H_

#include <string.h>

#ifdef __cplusplus
#include <type_traits>
#define EMBT_UNALIGNED_VALUE_TYPE(ptr) \
	typename std::remove_cv<typename std::remove_reference<decltype(*(ptr))>::type>::type
#else
#define EMBT_UNALIGNED_VALUE_TYPE(ptr) __typeof__(((void)0, *(ptr)))
#endif

/**
 * @brief Read a scalar or byte-structure value without dereferencing unaligned storage.
 * @param ptr Typed pointer into readable ordinary memory, evaluated once.
 *
 * The representation is copied unchanged and remains in native byte order.
 * Endian-specific protocols must use their byteorder helpers separately.
 * GNU type inference preserves existing scalar and structure-valued callers;
 * this is not an accessor for volatile registers or atomic storage.
 */
#define UNALIGNED_GET(ptr) \
	__extension__ ({ \
		EMBT_UNALIGNED_VALUE_TYPE(ptr) embt_unaligned_value; \
		const void *embt_unaligned_source = (ptr); \
		memcpy(&embt_unaligned_value, embt_unaligned_source, \
		       sizeof(embt_unaligned_value)); \
		embt_unaligned_value; \
	})

#endif /* EMBT_SYS_UNALIGNED_H_ */
