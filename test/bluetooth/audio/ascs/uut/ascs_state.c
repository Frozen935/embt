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

/** @file @brief Run the real ASCS implementation and flush its notification work. */
#include "../../../../../bluetooth/audio/ascs.c"

bool bt_test_ascs_flush_notifications(void)
{
	bool flushed = false;

	ARRAY_FOR_EACH_PTR(ascs.clients, client) {
		if (client->conn && bt_work_delayable_busy_get(&client->notify_work)) {
			struct bt_work_sync sync;

			bt_work_flush_delayable(&client->notify_work, &sync);
			flushed = true;
		}
	}
	return flushed;
}
