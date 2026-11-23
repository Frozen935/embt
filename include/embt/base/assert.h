/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *  Copyright (c) 2011-2014 Wind River Systems, Inc.
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

/** @file @brief Internal assertion adapter for stack components. */

#ifndef EMBT_BASE_ASSERT_H_
#define EMBT_BASE_ASSERT_H_

#include <embt/compiler.h>
#include <embt/base/util_macro.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Report an invariant failure and terminate, independently of log filters.
 * @param[in] condition Failing condition text, or NULL for a terse report.
 * @param[in] file Source file, or NULL for a terse report.
 * @param[in] line Source line.
 * @param[in] fmt Optional printf message, followed by arguments.
 * @note Thread context; bypasses stack log admission and work locks. Platform
 * output may use its native stream or syslog synchronization.
 */
FUNC_NORETURN __printf_like(4, 5)
void bt_assert_fail(const char *condition, const char *file, unsigned int line,
		    const char *fmt, ...);

#ifdef __cplusplus
}
#endif

/** @brief Check exactly once; evaluate message arguments only on failure. */
#define BT_ASSERT_CHECK(test, ...)                                          \
	do {                                                               \
		if (!(test)) {                                              \
			bt_assert_fail(#test, __FILE__, __LINE__,             \
				       COND_CODE_1(IS_EMPTY(__VA_ARGS__),    \
						   (NULL), (__VA_ARGS__)));     \
		}                                                          \
	} while (0)

#ifdef CONFIG_ASSERT
#define __ASSERT(test, ...) BT_ASSERT_CHECK(test, __VA_ARGS__)
#define __ASSERT_NO_MSG(test) BT_ASSERT_CHECK(test)
#else
#define __ASSERT(test, ...) ((void)0)
#define __ASSERT_NO_MSG(test) ((void)0)
#endif

#endif /* EMBT_BASE_ASSERT_H_ */
