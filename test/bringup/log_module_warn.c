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
 * @brief Warn module fixture for logging filter tests.
 */

#include <embt/base/log.h>
#include "test_log_modules.h"

LOG_MODULE_REGISTER(log_warn, 2);

int test_log_warn_emit(void)
{
	int evaluated = 0;

	LOG_INF("suppressed module info %d", ++evaluated);
	LOG_WRN("module warning %d", ++evaluated);
	return evaluated;
}
