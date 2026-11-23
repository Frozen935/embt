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
 * @brief Base lifecycle for the default worker and shared critical-section mutex.
 */

#include <embt/base/base.h>
#include <embt/base/work.h>
#include <embt/osi/io.h>
#include "private.h"

os_mutex_t bt_critical_mutex;
static bool initialized;

void bt_base_require(int result)
{
	if (result != 0) {
		osi_fatal("EmBT base operation failed");
	}
}

int bt_base_init(void)
{
	int err;

	/* The stack lifecycle entry serializes initialization and shutdown. */
	if (initialized) {
		return 0;
	}
	err = osi_mutex_init(&bt_critical_mutex, OS_MUTEX_RECURSIVE);
	if (err) {
		return err;
	}
	err = bt_work_queue_init(&bt_sys_work_q);
	if (!err) {
		err = bt_work_queue_start(&bt_sys_work_q, CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE,
			CONFIG_SYSTEM_WORKQUEUE_PRIORITY,
			&(const struct bt_work_queue_config){.name = "embt-system"});
	}
	if (err) {
		bt_work_queue_deinit(&bt_sys_work_q);
		osi_mutex_deinit(&bt_critical_mutex);
		return err;
	}
	initialized = true;
	return 0;
}

int bt_base_deinit(void)
{
	int err;

	if (!initialized) {
		return 0;
	}
	err = (bt_sys_work_q.started ? bt_work_queue_drain(&bt_sys_work_q, true) : 0);
	if (!err && bt_sys_work_q.joinable) {
		err = bt_work_queue_stop(&bt_sys_work_q);
	}
	if (err) {
		return err;
	}
	bt_base_require(bt_work_queue_deinit(&bt_sys_work_q));
	bt_base_require(osi_mutex_deinit(&bt_critical_mutex));
	initialized = false;
	return 0;
}
