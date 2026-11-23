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
 * @brief Off module fixture for logging filter tests.
 */

#include <embt/base/log.h>
#include "test_log_modules.h"

LOG_MODULE_REGISTER(log_off, 0);

int test_log_off_emit(void)
{
	int evaluated = 0;

	LOG_ERR("suppressed module error %d", ++evaluated);
	LOG_HEXDUMP_ERR(NULL, ++evaluated, "suppressed module dump");
	LOG_ERR_RATELIMIT_RATE(0, "suppressed module rate %d", ++evaluated);
	return evaluated;
}
