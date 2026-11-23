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
 * @brief Shared POSIX priority conversion with original caller numbering.
 */
#include "thread_config.h"
#include <errno.h>
#include <stdint.h>

int osi_thread_priority_map(int priority, int native_min, int native_max,
			    int *native)
{
	int64_t value;

	if (!native || native_min > native_max) {
		return -EINVAL;
	}
#if defined(__NuttX__)
	value = (int64_t)CONFIG_EMBT_THREAD_PRIORITY_BASE - (int64_t)priority;
#else
	(void)priority;
	value = 0;
#endif
	if (value < native_min || value > native_max) {
		return -ERANGE;
	}
	*native = (int)value;
	return 0;
}
