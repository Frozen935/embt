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

/** @file @brief One entrypoint shared by all registered cmocka programs. */
#include "test.h"

extern const struct bt_test_suite *const bt_test_registry[];
extern const size_t bt_test_registry_count;

int main(int argc, char **argv)
{
	return bt_test_main(argc, argv, bt_test_registry, bt_test_registry_count);
}
