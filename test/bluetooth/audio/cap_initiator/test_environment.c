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

/** @file @brief Module-local environment required by all registered suites. */
#include "support/test_support.h"
#include "test_environment.h"

int bt_cap_initiator_base_init(void);
int bt_cap_common_base_init(void);

int bt_test_audio_cap_initiator_environment_setup(void **state)
{
	static bool initialized;
	int result = bt_test_base_setup(state);

	if (result || initialized) {
		return result;
	}
	result = bt_cap_initiator_base_init();
	if (result) {
		return result;
	}
	result = bt_cap_common_base_init();
	if (result) {
		return result;
	}
	initialized = true;
	return 0;
}
