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


/** @file @brief Monotonic deadlines for base mechanisms. */

#include <embt/base/time.h>
#include <embt/base/base.h>
#include <embt/base/util.h>
#include <embt/osi/time.h>
#include <errno.h>

int64_t bt_uptime_get_ms(void)
{
	uint64_t ms;

	bt_base_require(osi_time_get_ms(&ms));
	bt_base_require(ms > INT64_MAX ? -EOVERFLOW : 0);
	return (int64_t)ms;
}

uint64_t bt_uptime_get_us(void)
{
	uint64_t us;

	bt_base_require(osi_time_get_us(&us));
	return us;
}

uint32_t bt_uptime_get_ms_32(void)
{
	return (uint32_t)bt_uptime_get_ms();
}

bt_timepoint_t bt_timepoint_calc(bt_timeout_t timeout)
{
	int64_t now = bt_uptime_get_ms();

	return (bt_timepoint_t){.deadline_ms = timeout < 0 ? INT64_MAX
							  : (timeout > INT64_MAX - now
								     ? INT64_MAX
								     : now + timeout)};
}

bt_timeout_t bt_timepoint_timeout(bt_timepoint_t end)
{
	if (end.deadline_ms == INT64_MAX) {
		return BT_FOREVER;
	}
	return BT_MSEC(MAX(INT64_C(0), end.deadline_ms - bt_uptime_get_ms()));
}

bool bt_timepoint_expired(bt_timepoint_t end)
{
	return end.deadline_ms != INT64_MAX && bt_uptime_get_ms() >= end.deadline_ms;
}
