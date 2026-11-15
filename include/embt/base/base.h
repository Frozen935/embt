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
 * @brief Explicit base lifecycle and fatal invariant handling.
 */

#ifndef EMBT_BASE_BASE_H_
#define EMBT_BASE_BASE_H_


/**
 * @brief Initialize process-lifetime base locks and the default worker.
 *
 * Call from the serialized stack lifecycle owner. No transport is opened.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_base_init(void);
/**
 * @brief Stop and join the default worker after clients quiesce.
 *
 * No queue, pool or work API may run concurrently with this operation.
 * @return Zero on success, otherwise a negative errno value.
 */
int bt_base_deinit(void);
/**
 * @brief Terminate on an error that an inherited void API cannot report.
 *
 * Nonzero results are fatal; zero returns normally.
 *
 * @param[in] result Status to check.
 */
void bt_base_require(int result);

#endif /* EMBT_BASE_BASE_H_ */
