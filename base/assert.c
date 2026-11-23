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

/** @file @brief Bounded assertion failure report, independent of stack log admission. */
#include <embt/base/assert.h>
#include <embt/base/log.h>
#include <embt/osi/io.h>
#include <stdarg.h>
#include <stdio.h>

FUNC_NORETURN void bt_assert_fail(const char *condition, const char *file, unsigned int line,
				 const char *fmt, ...)
{
	char message[512] = "ASSERTION FAIL";
	int prefix;
	size_t used;

	if (condition && file) {
		prefix = snprintf(message, sizeof(message), "ASSERTION FAIL [%s] @ %s:%u",
				  condition, file, line);
	} else {
		prefix = snprintf(message, sizeof(message), "ASSERTION FAIL");
	}
	used = prefix < 0 ? 0 : (size_t)prefix;
	if (used >= sizeof(message)) {
		used = sizeof(message) - 1;
	}
	if (fmt && fmt[0] && used < sizeof(message) - 1) {
		va_list args;

		message[used++] = '\n';
		va_start(args, fmt);
		(void)vsnprintf(message + used, sizeof(message) - used, fmt, args);
		va_end(args);
	}
	message[sizeof(message) - 1] = '\0';
	bt_log_print("%s\n", message);
	osi_fatal(NULL);
}
