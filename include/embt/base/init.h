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

/** @file @brief Explicit initialization ordering within a caller-defined dependency stage. */
#ifndef EMBT_BASE_INIT_H_
#define EMBT_BASE_INIT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief One initializer and its reverse cleanup; both functions own partial-resource cleanup. */
struct bt_init_entry {
	int priority;
	int (*initialize)(void);
	int (*deinitialize)(void);
};

/**
 * @brief Initialize one dependency stage in stable ascending priority order.
 * @param[in] entries Descriptors valid throughout rollback; initialize a fresh stage only.
 * @param[in] count Number of entries.
 * @param[out] order Caller-owned storage for count indices; records actual execution order.
 * @param[out] completed Successful initializer count; initialized to zero before execution.
 * @return Zero on success, -EINVAL for an invalid descriptor, or the initializer's error.
 * @note Equal priorities preserve declaration order. The caller performs rollback on failure.
 */
int bt_init_run(const struct bt_init_entry *entries, size_t count,
		size_t *order, size_t *completed);

/**
 * @brief Release successfully initialized entries in reverse execution order.
 * @param[in] entries The same descriptors supplied to bt_init_run().
 * @param[in] order Recorded execution indices.
 * @param[in,out] completed Remaining successful entries; updated after each successful cleanup.
 * @return Zero on success or the first cleanup error, retaining its index for a later retry.
 */
int bt_init_rollback(const struct bt_init_entry *entries, const size_t *order, size_t *completed);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_BASE_INIT_H_ */
