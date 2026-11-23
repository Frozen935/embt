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
 * @brief Logging fixtures compiled with independent module levels.
 */

#ifndef EMBT_TEST_LOG_MODULES_H_
#define EMBT_TEST_LOG_MODULES_H_

/** @brief Emit Debug messages and return the evaluated argument count. */
int test_log_debug_emit(void);

/** @brief Emit from a separate translation unit sharing the Debug module. */
int test_log_declared_emit(void);

/** @brief Emit a Warning while suppressing an Info message. */
int test_log_warn_emit(void);

/** @brief Attempt disabled logs and return the evaluated argument count. */
int test_log_off_emit(void);

#endif /* EMBT_TEST_LOG_MODULES_H_ */
