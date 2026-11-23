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
 * @brief OSI coordination and nonempty cmocka execution.
 */

#include "test_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void (*selected_case)(void **);
static bool entered;

/**
 * @brief Observe actual invocation even if external cmocka filters suppress a case.
 * @param[in,out] state Cmocka fixture state.
 */
static void invoke_case(void **state)
{
	entered = true;
	selected_case(state);
}

int test_run(int argc, char **argv, const struct CMUnitTest *tests, size_t count)
{
	int failures = 0;
	size_t executed = 0;

	if (argc > 2 || !tests || !count) {
		fprintf(stderr, "Select one registered case or run the full module\n");
		return 2;
	}
	for (size_t i = 0; i < count; i++) {
		struct CMUnitTest selected = tests[i];
		int result;

		if (argc == 2 && strcmp(argv[1], selected.name)) {
			continue;
		}
		selected_case = selected.test_func;
		selected.test_func = invoke_case;
		entered = false;
		result = _cmocka_run_group_tests("osi", &selected, 1, NULL, NULL);
		if (!entered) {
			fprintf(stderr, "Registered case did not run: %s\n", selected.name);
			failures++;
		}
		failures += result != 0;
		executed++;
	}
	if (!executed) {
		fprintf(stderr, "Unknown OSI case\n");
		return 2;
	}
	return failures ? 1 : 0;
}

void test_require(int result)
{
	if (result) {
		fprintf(stderr, "OSI test coordination failed: %d\n", result);
		exit(EXIT_FAILURE);
	}
}

uint64_t test_now_ms(void)
{
	uint64_t value;

	test_require(osi_time_get_ms(&value));
	return value;
}

void test_wait(os_sem_t *sem)
{
	test_require(osi_sem_take(sem, TEST_TIMEOUT_MS));
}
