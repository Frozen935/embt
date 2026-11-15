/* Copyright (c) 2022 Nordic Semiconductor ASA
 * Copyright (C) 2026 EmBT contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BT_COMMON_ASSERT_H_
#define BT_COMMON_ASSERT_H_

#include <embt/base/assert.h>

/* Bluetooth invariants remain active when only BT_ASSERT is enabled.
 * POSIX panic and oops share the base terminal action.
 */
#ifdef CONFIG_BT_ASSERT
#ifdef CONFIG_BT_ASSERT_VERBOSE
#define BT_ASSERT(condition) BT_ASSERT_CHECK(condition)
#define BT_ASSERT_MSG(condition, ...) BT_ASSERT_CHECK(condition, __VA_ARGS__)
#else
#define BT_ASSERT(condition)                                            \
	do {                                                            \
		if (!(condition)) {                                     \
			bt_assert_fail(NULL, NULL, 0, NULL);              \
		}                                                      \
	} while (0)
#define BT_ASSERT_MSG(condition, ...) BT_ASSERT(condition)
#endif
#else
#define BT_ASSERT(condition) __ASSERT_NO_MSG(condition)
#define BT_ASSERT_MSG(condition, ...) __ASSERT(condition, __VA_ARGS__)
#endif

#endif /* BT_COMMON_ASSERT_H_ */
