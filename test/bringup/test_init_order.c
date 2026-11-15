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

/** @file @brief Stable configurable initialization order and reverse rollback. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <embt/base/init.h>
#include <errno.h>
#include <stdbool.h>

static int trace[16];
static size_t length;
static bool fail_b, fail_cleanup;

/** @brief Reset one isolated initialization-stage fixture. */
static int setup(void **state)
{
	(void)state;
	length = 0;
	fail_b = fail_cleanup = false;
	return 0;
}

static int init_a(void) { trace[length++] = 1; return 0; }
static int init_b(void) { trace[length++] = 2; return fail_b ? -EIO : 0; }
static int init_c(void) { trace[length++] = 3; return 0; }
static int close_a(void) { trace[length++] = -1; return 0; }
static int close_b(void) { trace[length++] = -2; return 0; }
static int close_c(void) { trace[length++] = -3; return fail_cleanup ? -EBUSY : 0; }

/** @brief Equal priorities preserve declaration order, and cleanup reverses actual execution. */
static void stable_order(void **state)
{
	const struct bt_init_entry entries[] = {{10, init_a, close_a},
		{20, init_b, close_b}, {10, init_c, close_c}};
	size_t order[3], completed;
	const int expected[] = {1, 3, 2, -2, -3, -1};
	(void)state;
	assert_int_equal(bt_init_run(entries, 3, order, &completed), 0);
	assert_int_equal(completed, 3);
	assert_int_equal(bt_init_rollback(entries, order, &completed), 0);
	assert_int_equal(completed, 0);
	assert_memory_equal(trace, expected, sizeof(expected));
}

/** @brief Custom ordering applies independently of descriptor declaration order. */
static void custom_order(void **state)
{
	const struct bt_init_entry entries[] = {{90, init_a, close_a},
		{10, init_b, close_b}, {50, init_c, close_c}};
	size_t order[3], completed;
	const int expected[] = {2, 3, 1};
	(void)state;
	assert_int_equal(bt_init_run(entries, 3, order, &completed), 0);
	assert_memory_equal(trace, expected, sizeof(expected));
	assert_int_equal(bt_init_rollback(entries, order, &completed), 0);
}

/** @brief A failed initializer excludes itself and later entries from rollback. */
static void failure_order(void **state)
{
	const struct bt_init_entry entries[] = {{10, init_a, close_a},
		{30, init_b, close_b}, {20, init_c, close_c}};
	size_t order[3], completed;
	const int expected[] = {1, 3, 2, -3, -1};
	(void)state;
	fail_b = true;
	assert_int_equal(bt_init_run(entries, 3, order, &completed), -EIO);
	assert_int_equal(completed, 2);
	assert_int_equal(bt_init_rollback(entries, order, &completed), 0);
	assert_memory_equal(trace, expected, sizeof(expected));
}

/** @brief Cleanup failures keep ownership recorded until a successful retry. */
static void cleanup_retry(void **state)
{
	const struct bt_init_entry entries[] = {{10, init_a, close_a}, {20, init_c, close_c}};
	size_t order[2], completed;
	(void)state;
	assert_int_equal(bt_init_run(entries, 2, order, &completed), 0);
	fail_cleanup = true;
	assert_int_equal(bt_init_rollback(entries, order, &completed), -EBUSY);
	assert_int_equal(completed, 2);
	fail_cleanup = false;
	assert_int_equal(bt_init_rollback(entries, order, &completed), 0);
	assert_int_equal(completed, 0);
}

/** @brief Invalid descriptors fail before side effects, and an empty stage is permitted. */
static void invalid_entries(void **state)
{
	const struct bt_init_entry entries[] = {{10, init_a, close_a}, {20, NULL, close_b}};
	size_t order[2], completed;
	(void)state;
	assert_int_equal(bt_init_run(entries, 2, order, &completed), -EINVAL);
	assert_int_equal(length, 0);
	assert_int_equal(bt_init_run(NULL, 0, NULL, &completed), 0);
	assert_int_equal(bt_init_rollback(NULL, NULL, &completed), 0);
}

/** @brief Run the explicit initialization-stage contracts. */
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(stable_order, setup),
		cmocka_unit_test_setup(custom_order, setup),
		cmocka_unit_test_setup(failure_order, setup),
		cmocka_unit_test_setup(cleanup_retry, setup),
		cmocka_unit_test_setup(invalid_entries, setup),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
