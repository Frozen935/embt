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

/** @file @brief Internal original-to-native thread priority conversion. */
#ifndef EMBT_OSI_THREAD_CONFIG_H_
#define EMBT_OSI_THREAD_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate a platform conversion of the original priority number.
 * @param[in] priority Original caller priority; smaller numbers denote higher priority.
 * @param[in] native_min Native policy minimum.
 * @param[in] native_max Native policy maximum.
 * @param[out] native Converted priority; unchanged on error.
 * @return Zero on success, -ERANGE for an invalid result, or -EINVAL for invalid output/ranges.
 * @note Linux's conversion yields its default priority; Linux creation uses default attributes.
 *   The shared NuttX creation path calls this once with native limits before applying the result.
 */
int osi_thread_priority_map(int priority, int native_min, int native_max,
			    int *native);

#ifdef __cplusplus
}
#endif

#endif /* EMBT_OSI_THREAD_CONFIG_H_ */
