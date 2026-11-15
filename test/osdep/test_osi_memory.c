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
 * @brief System allocation, zero initialization, alignment and release tests.
 */

#include "support/test_support.h"

#include <string.h>
/**
 * @brief Verify memory allocations behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_memory_allocations(void **state)
{
	unsigned char *memory = osi_malloc(37);
	unsigned char *zero = osi_calloc(17, 3);
	void *aligned = osi_aligned_alloc(64, 37);
	(void)state;
	assert_non_null(memory);
	assert_non_null(zero);
	assert_non_null(aligned);
	memset(memory, 0x5a, 37);
	for (size_t i = 0; i < 37; i++) {
		assert_int_equal(memory[i], 0x5a);
	}
	for (size_t i = 0; i < 51; i++) {
		assert_int_equal(zero[i], 0);
	}
	assert_int_equal((uintptr_t)aligned % 64, 0);
	memset(aligned, 0x31, 37);
	osi_free(aligned);
	osi_free(zero);
	osi_free(memory);
	osi_free(NULL);
}
/**
 * @brief Verify memory invalid behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_memory_invalid(void **state)
{
	(void)state;
	assert_null(osi_malloc(0));
	assert_null(osi_calloc(0, 1));
	assert_null(osi_calloc(1, 0));
	assert_null(osi_calloc(SIZE_MAX, 2));
	assert_null(osi_aligned_alloc(0, 10));
	assert_null(osi_aligned_alloc(3, 10));
	assert_null(osi_aligned_alloc(64, 0));
}

/**
 * @brief Run one registered case in its own process.
 * @param[in] argc Argument count.
 * @param[in] argv Arguments containing the exact case name.
 * @return Test result or a selection error.
 */
int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_memory_allocations),
		cmocka_unit_test(test_memory_invalid),
	};

	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
