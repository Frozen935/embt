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

/** @file @brief Private lifecycle state for the retained critical-section mutex. */
#ifndef EMBT_BASE_PRIVATE_H_
#define EMBT_BASE_PRIVATE_H_
#include <embt/base/base.h>
#include <embt/osi/sync.h>

extern os_mutex_t bt_critical_mutex;

#endif /* EMBT_BASE_PRIVATE_H_ */
