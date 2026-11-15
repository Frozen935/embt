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
 * @brief OSI-only coordination and cmocka contract execution.
 */

#ifndef EMBT_OSI_TEST_SUPPORT_H_
#define EMBT_OSI_TEST_SUPPORT_H_

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <embt/osi/osi.h>

#define TEST_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define TEST_TIMEOUT_MS INT64_C(2000)

/**
 * @brief Run all registered cases or one exact selection and reject empty execution.
 * @param[in] argc Argument count.
 * @param[in] argv Optional exact case name in argv[1].
 * @param[in] tests Registered cases.
 * @param[in] count Number of cases.
 * @return Zero on success or nonzero for a failure, missing selection or filtered-out case.
 */
int test_run(int argc, char **argv, const struct CMUnitTest *tests, size_t count);

/**
 * @brief Require successful coordination without a worker-side cmocka longjmp.
 * @param[in] result OSI result.
 * @note An infrastructure error ends this test application with a failure status.
 */
void test_require(int result);

/**
 * @brief Read monotonic milliseconds through the OSI contract.
 * @return OSI timestamp; an interface error ends the test application.
 */
uint64_t test_now_ms(void);

/**
 * @brief Consume an OSI notification with a bounded wait.
 * @param[in,out] sem Initialized notification semaphore.
 * @note A failed coordination ends the application; the runner supervises hangs independently.
 */
void test_wait(os_sem_t *sem);

#endif /* EMBT_OSI_TEST_SUPPORT_H_ */
