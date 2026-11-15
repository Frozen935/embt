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
 * @brief Internal OSI object types and storage initialization.
 */

#ifndef EMBT_OSI_TYPES_H_
#define EMBT_OSI_TYPES_H_

#include <osi/common.h>

#include <osi/native_types.h>

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

#endif /* EMBT_OSI_TYPES_H_ */
