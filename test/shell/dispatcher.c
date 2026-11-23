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

/** @file @brief Verify native shell argument-count descriptor semantics. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <shell/bt_shell.h>

static size_t calls, observed_argc;

static int handle(const struct bt_shell *sh, size_t argc, char **argv)
{
	(void)sh;
	(void)argv;
	calls++;
	observed_argc = argc;
	return 42;
}

/** @brief Zero mandatory count leaves validation to the selected handler. */
static void descriptor_counts(void **state)
{
	const struct bt_shell_cmd_entry uncounted = BT_SHELL_CMD_ARG(apply, NULL, "", handle, 0, 0);
	const struct bt_shell_cmd_entry counted = BT_SHELL_CMD_ARG(checked, NULL, "", handle, 2, 1);
	struct bt_shell sh;
	FILE *output = tmpfile();
	char *apply[] = {"apply", "extra", "another"};
	char *checked[] = {"checked", "one", "two", "three"};
	(void)state;
	assert_non_null(output);
	bt_shell_init(&sh, output);
	assert_int_equal(bt_shell_register(&sh, &uncounted), 0);
	assert_int_equal(bt_shell_register(&sh, &counted), 0);
	assert_int_equal(bt_shell_exec(&sh, 1, apply), 42);
	assert_int_equal(observed_argc, 1);
	assert_int_equal(bt_shell_exec(&sh, 3, apply), 42);
	assert_int_equal(observed_argc, 3);
	assert_int_equal(bt_shell_exec(&sh, 1, checked), -EINVAL);
	assert_int_equal(bt_shell_exec(&sh, 2, checked), 42);
	assert_int_equal(bt_shell_exec(&sh, 3, checked), 42);
	assert_int_equal(bt_shell_exec(&sh, 4, checked), -EINVAL);
	assert_int_equal(calls, 4);
	assert_int_equal(fclose(output), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(descriptor_counts)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
