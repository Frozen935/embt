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
 * @brief Native one-shot monotonic timers with borrowed notification storage.
 */

#include <embt/osi/timer.h>
#include <signal.h>
#include "private.h"

/**
 * @brief Forward a native notification while borrowing process-lifetime timer storage.
 * @param[in] value Native notification value containing the OSI timer address.
 */
static void timer_notify(union sigval value)
{
	os_timer_t *timer = value.sival_ptr;

	timer->callback(timer, timer->argument);
}

int osi_timer_create(os_timer_t *timer, os_timer_cb_t callback, void *argument)
{
	struct sigevent event = {0};

	if (!timer || !callback) {
		return -EINVAL;
	}
	timer->callback = callback;
	timer->argument = argument;
	event.sigev_notify = SIGEV_THREAD;
	event.sigev_notify_function = timer_notify;
	event.sigev_value.sival_ptr = timer;
	return timer_create(CLOCK_MONOTONIC, &event, &timer->native) == 0 ? 0 : -errno;
}

int osi_timer_start(os_timer_t *timer, os_timeout_t timeout_ms)
{
	struct itimerspec schedule = {0};
	int rc;

	if (!timer || timeout_ms <= 0) {
		return -EINVAL;
	}
	rc = osi_posix_interval(timeout_ms, &schedule.it_value);
	if (rc != 0) {
		return rc;
	}
	return timer_settime(timer->native, 0, &schedule, NULL) == 0 ? 0 : -errno;
}

int osi_timer_stop(os_timer_t *timer)
{
	struct itimerspec schedule = {0};

	if (!timer) {
		return -EINVAL;
	}
	return timer_settime(timer->native, 0, &schedule, NULL) == 0 ? 0 : -errno;
}

int osi_timer_delete(os_timer_t *timer)
{
	if (!timer) {
		return -EINVAL;
	}
	/* Keep callback fields intact: native deletion is not a callback drain. */
	return timer_delete(timer->native) == 0 ? 0 : -errno;
}

int osi_timer_remaining_ms(os_timer_t *timer, uint64_t *milliseconds)
{
	struct itimerspec schedule;
	uint64_t fraction;

	if (!timer || !milliseconds) {
		return -EINVAL;
	}
	if (timer_gettime(timer->native, &schedule) != 0) {
		return -errno;
	}
	fraction = ((uint64_t)schedule.it_value.tv_nsec + 999999) / 1000000;
	if ((uintmax_t)schedule.it_value.tv_sec > (UINT64_MAX - fraction) / 1000) {
		return -EOVERFLOW;
	}
	*milliseconds = (uint64_t)schedule.it_value.tv_sec * 1000 + fraction;
	return 0;
}
