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
 * @brief Shared cmocka runner and bounded native test coordination.
 */

#ifndef EMBT_OSI_TEST_SUPPORT_H_
#define EMBT_OSI_TEST_SUPPORT_H_
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <osi/osi.h>
#include <stdatomic.h>
#include <unistd.h>
#include <stdint.h>
#define TEST_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
/**
 * @brief Execute exactly one registered case, rejecting unknown filters.
 * @param[in] argc Argument count.
 * @param[in] argv Arguments, with the exact case name in argv[1].
 * @param[in] tests Registered cmocka cases.
 * @param[in] count Number of cases.
 * @return cmocka result, or a nonzero status for an invalid selection.
 */
int test_run(int argc, char **argv, const struct CMUnitTest *tests, size_t count);
/** @brief Read the native monotonic clock for an independent timing reference.
 * @return Monotonic milliseconds, or terminate the test process on a clock error.
 */
uint64_t test_now_ms(void);
/** @brief Wait at most two seconds for native coordination.
 * @param[in,out] sem Native semaphore to consume.
 * @note A failure terminates the process instead of releasing live worker context.
 */
void test_wait(sem_t *sem);
/** @brief Wait for a callback count using a bounded native clock interval.
 * @param[in] count Atomic callback count.
 * @param[in] expected Minimum count to observe.
 * @return true when the count is observed, otherwise false after the deadline.
 */
bool test_wait_count(atomic_uint *count, unsigned int expected);
#endif
