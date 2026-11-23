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
 * @brief Verify compiler attributes and unaligned value extraction.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdint.h>
#include <string.h>
#include <embt/compiler.h>
#include <embt/base/unaligned.h>

struct packet_header {
	uint8_t type;
	uint32_t value;
} __packed;

struct bytes3 {
	uint8_t bytes[3];
};

static uint8_t aligned_storage[32] __aligned(16);
static unsigned int pointer_calls;

BUILD_ASSERT(sizeof(struct packet_header) == 5, "Packed size must be preserved");
BUILD_ASSERT(offsetof(struct packet_header, value) == 1, "Packed offset must be preserved");
BUILD_ASSERT(sizeof(struct bytes3) == 3);

static const uint64_t *read_pointer(const uint8_t *storage)
{
	pointer_calls++;
	return (const uint64_t *)storage;
}

static void test_layout(void **state)
{
	(void)state;
	assert_int_equal(sizeof(struct packet_header), 5);
	assert_int_equal(offsetof(struct packet_header, value), 1);
	assert_int_equal((uintptr_t)aligned_storage % 16, 0);
}

static void test_scalar(void **state)
{
	uint8_t storage[16] __aligned(8);
	uint64_t expected = UINT64_C(0x1122334455667788);
	uint64_t actual;

	(void)state;
	memset(storage, 0xa5, sizeof(storage));
	memcpy(storage + 1, &expected, sizeof(expected));
	pointer_calls = 0;
	actual = UNALIGNED_GET(read_pointer(storage + 1));
	assert_int_equal(actual, expected);
	assert_int_equal(pointer_calls, 1);
	assert_int_equal(storage[0], 0xa5);
	assert_int_equal(storage[9], 0xa5);
}

static void test_structure(void **state)
{
	const uint8_t storage[] = {0xff, 0x12, 0x34, 0x56, 0xee};
	struct bytes3 actual;

	(void)state;
	actual = UNALIGNED_GET((const struct bytes3 *)(storage + 1));
	assert_memory_equal(actual.bytes, storage + 1, 3);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_layout),
		cmocka_unit_test(test_scalar),
		cmocka_unit_test(test_structure),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
