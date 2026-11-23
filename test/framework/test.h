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

/** @file @brief Explicit cmocka suite metadata and shared execution. */
#ifndef EMBT_TEST_FRAMEWORK_H_
#define EMBT_TEST_FRAMEWORK_H_

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdbool.h>
#include <cmocka.h>

/** @brief A compile-time description; fixtures own all runtime resources. */
struct bt_test_suite {
	const char *name;
	const struct CMUnitTest *cases;
	size_t case_count;
	CMFixtureFunction setup;
	CMFixtureFunction teardown;
};

/**
 * @brief Define an externally referenced suite without startup registration.
 * @param _id Identifier used by the CMake registry and the displayed suite name.
 * @param _cases Actual CMUnitTest array, not a pointer.
 * @param _setup Group fixture setup, or NULL.
 * @param _teardown Group fixture teardown, or NULL.
 */
#define BT_TEST_SUITE(_id, _cases, _setup, _teardown)                 \
	const struct bt_test_suite bt_test_suite_##_id = {           \
		.name = #_id,                                      \
		.cases = (_cases),                                 \
		.case_count = sizeof(_cases) / sizeof((_cases)[0]), \
		.setup = (_setup),                                 \
		.teardown = (_teardown),                           \
	}

/** @brief Record the controlling thread without initializing a Bluetooth stack. */
void bt_test_runtime_initialize(void);
/** @brief Whether the calling thread can safely use cmocka longjmp assertions. */
bool bt_test_is_control_thread(void);
/** @brief Execute one suite; return 0 on success, 1 on failure, or 2 for an unknown case. */
int bt_test_run_suite(const struct bt_test_suite *suite, const char *case_name);
/** @brief Parse the common test CLI and execute or list an explicit registry. */
int bt_test_main(int argc, char **argv,
		 const struct bt_test_suite *const *registry, size_t count);
#endif
