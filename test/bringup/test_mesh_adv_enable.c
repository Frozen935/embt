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

/** @file @brief Verify legacy Mesh enable retry and idempotence without a radio. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <embt/osi/thread.h>
#include "mesh/adv.h"

static int create_result = -EAGAIN;
static unsigned int create_count;

int __wrap_osi_thread_create(os_thread_t *thread, void (*entry)(void *), void *argument,
			    const char *name, int priority, size_t stack_size)
{
	(void)thread;
	(void)entry;
	(void)argument;
	(void)name;
	(void)priority;
	(void)stack_size;
	create_count++;
	return create_result;
}

/** @brief Creation failures remain errors, while repeated successful enable is a no-op. */
static void enable_retry(void **state)
{
	(void)state;
	assert_int_equal(bt_mesh_adv_enable(), -EAGAIN);
	create_result = 0;
	assert_int_equal(bt_mesh_adv_enable(), 0);
	assert_int_equal(bt_mesh_adv_enable(), 0);
	assert_int_equal(create_count, 2);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(enable_retry)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
