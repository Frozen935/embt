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
 * @brief Bounded native waits and a nonempty cmocka case runner.
 */

#include "test_support.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int test_run(int argc, char **argv, const struct CMUnitTest *tests, size_t count)
{
	if (argc != 2) {
		fprintf(stderr, "Select one exact registered test case\n");
		return 2;
	}
	for (size_t i = 0; i < count; i++) {
		if (strcmp(argv[1], tests[i].name) == 0) {
			unsetenv("CMOCKA_TEST_FILTER");
			unsetenv("CMOCKA_SKIP_FILTER");
			cmocka_set_test_filter(tests[i].name);
			return _cmocka_run_group_tests("osi", &tests[i], 1, NULL, NULL);
		}
	}
	fprintf(stderr, "Unknown test case: %s\n", argv[1]);
	return 2;
}

uint64_t test_now_ms(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		exit(1);
	}
	return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

void test_wait(sem_t *sem)
{
	struct timespec deadline;
	int rc;

	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += 2;
	do {
		rc = sem_clockwait(sem, CLOCK_MONOTONIC, &deadline);
	} while (rc != 0 && errno == EINTR);
	if (rc != 0) {
		/* Do not longjmp past a worker that might still reference stack storage. */
		fprintf(stderr, "Native coordination failed: %d\n", errno);
		exit(1);
	}
}

bool test_wait_count(atomic_uint *count, unsigned int expected)
{
	uint64_t deadline = test_now_ms() + 2000;
	struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000};

	while (atomic_load(count) < expected && test_now_ms() < deadline) {
		nanosleep(&pause, NULL);
	}
	return atomic_load(count) >= expected;
}
