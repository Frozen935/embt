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

/** @file @brief Contract scenarios for the generic runner and native fixtures. */
#include "test.h"
#include <stdio.h>
#include <embt/osi/thread.h>

static int fixture_value;
static int group_setup(void **state)
{
	puts("group_setup");
	fixture_value = 10;
	*state = &fixture_value;
	return 0;
}
static int group_teardown(void **state)
{
	assert_ptr_equal(*state, &fixture_value);
	puts("group_teardown");
	return 0;
}
static int case_setup(void **state)
{
	assert_ptr_equal(*state, &fixture_value);
	fixture_value++;
	puts("case_setup");
	return 0;
}
static int case_teardown(void **state)
{
	assert_ptr_equal(*state, &fixture_value);
	puts("case_teardown");
	return 0;
}
static void positive__first(void **state)
{
	assert_ptr_equal(*state, &fixture_value);
	assert_int_equal(fixture_value, 11);
}
static void positive__second(void **state)
{
	assert_true(*(int *)*state >= 11);
}
static void negative__failure(void **state)
{
	(void)state;
	assert_int_equal(1, 2);
}
static void negative__skip(void **state)
{
	(void)state;
	skip();
}
static void worker(void *state)
{
	(void)state;
	assert_int_equal(1, 2);
}
static void negative__worker_failure(void **state)
{
	(void)state;
	os_thread_t thread;

	assert_int_equal(osi_thread_create(&thread, worker, NULL, "test-fail", 0, 0), 0);
	osi_thread_join(&thread);
	fail();
}
static const struct CMUnitTest positive_cases[] = {
	cmocka_unit_test_setup_teardown(positive__first, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(positive__second, case_setup, case_teardown),
};
static const struct CMUnitTest negative_cases[] = {
	cmocka_unit_test(negative__failure),
	cmocka_unit_test(negative__skip),
	cmocka_unit_test(negative__worker_failure),
};
BT_TEST_SUITE(positive, positive_cases, group_setup, group_teardown);
BT_TEST_SUITE(negative, negative_cases, NULL, NULL);
