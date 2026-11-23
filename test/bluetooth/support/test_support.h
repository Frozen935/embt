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

/** @file @brief Cmocka support for migrated Bluetooth test programs. */
#ifndef EMBT_BT_TEST_SUPPORT_H_
#define EMBT_BT_TEST_SUPPORT_H_
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <framework/test.h>
#include <embt/base/util.h>
#include <embt/base/time.h>
#include <embt/base/work.h>
#include <embt/base/time.h>
#include <embt/base/fifo.h>
#include <embt/osi/sync.h>
#include <embt/osi/thread.h>
#include <embt/osi/time.h>
static inline uint64_t bt_test_uptime_us(void)
{
 uint64_t microseconds;
 if (osi_time_get_us(&microseconds)) { abort(); }
 return microseconds;
}
struct bt_test_case_info { const char *suite; const char *name; };
/** @brief Require a successful fixture operation, terminating on infrastructure failure. */
void bt_test_require(int result);
/**
 * @brief Run a worker entry with a standard-C assertion boundary on the same thread.
 * @param[in] entry Worker operation expected to assert.
 * @param[in,out] argument Borrowed operation context.
 * @return true when an assertion was caught, or false if the entry returned normally.
 * @note This test boundary does not perform application-resource cleanup.
 */
bool bt_test_thread_assert(os_thread_entry_t entry, void *argument);
/** @brief Select an unopened descriptor; any unexpected send fails the fixture. */
void bt_test_select_unopened_hci(void);
/** @brief Prepare real base mechanisms and the controlling test thread. */
int bt_test_initialize(void);
/** @brief Prepare the real base/PSA environment for a suite. */
int bt_test_base_setup(void **state);
/** @brief Prepare the real Host environment for a suite. */
int bt_test_host_setup(void **state);
/** @brief Prepare Host or raw pools according to the canonical configuration. */
int bt_test_buffer_setup(void **state);
#endif
