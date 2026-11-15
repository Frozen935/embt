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
 * @brief Thread-local, one-shot native failure injection for test executables.
 */

#ifndef EMBT_OSI_TEST_FAULTS_H_
#define EMBT_OSI_TEST_FAULTS_H_
#include <stdatomic.h>
enum test_fault {
	F_NONE,
	F_MALLOC,
	F_CALLOC,
	F_ALIGN,
	F_CREATE,
	F_ATTR_INIT,
	F_MUTEX_ATTR_INIT,
	F_MUTEX_ATTR_TYPE,
	F_MUTEX_INIT,
	F_COND_ATTR_INIT,
	F_COND_ATTR_CLOCK,
	F_COND_INIT,
	F_SEM_INIT,
	F_SEM_VALUE,
	F_SEM_POST,
	F_SEM_WAIT,
	F_CLOCK,
	F_CLOCK_LIMIT,
	F_SLEEP,
	F_TIMER_CREATE,
	F_TIMER_SET,
	F_TIMER_GET,
	F_TIMER_DELETE,
	F_WRITE,
	F_WRITE_ZERO,
	F_WRITE_PARTIAL,
};
/** @brief Arm one failure on the calling thread's next matching native operation.
 * @param[in] fault Operation to fail.
 * @param[in] code Positive native error code.
 */
void test_fault_arm(enum test_fault fault, int code);
extern atomic_int test_live_allocations;
extern atomic_uint test_attr_destroyed, test_mutexattr_destroyed;
extern atomic_uint test_condattr_destroyed, test_mutex_destroyed;
#endif
