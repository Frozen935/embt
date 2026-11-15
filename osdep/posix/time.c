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
 * @brief POSIX monotonic time, finite sleep and timeout conversion.
 */

#include <embt/osi/time.h>
#include <limits.h>
#include <stdint.h>
#include "private.h"

/* Use the compiler target's time_t width and signedness, not the Python host. */
#define OSI_TIME_MAX                                                                               \
	(UINTMAX_MAX >>                                                                            \
	 (sizeof(uintmax_t) * CHAR_BIT - sizeof(time_t) * CHAR_BIT + ((time_t) - 1 < 0)))

int osi_posix_interval(os_timeout_t milliseconds, struct timespec *interval)
{
	struct timespec value;
	uintmax_t seconds;

	if (!interval || milliseconds < 0) {
		return -EINVAL;
	}
	seconds = (uintmax_t)milliseconds / 1000;
	if (seconds > OSI_TIME_MAX) {
		return -EOVERFLOW;
	}
	value.tv_sec = (time_t)seconds;
	value.tv_nsec = (long)(milliseconds % 1000) * 1000000;
	*interval = value;
	return 0;
}

int osi_posix_deadline(os_timeout_t milliseconds, struct timespec *deadline)
{
	struct timespec now, interval, value;
	uintmax_t seconds;
	int rc;

	if (!deadline) {
		return -EINVAL;
	}
	rc = osi_posix_interval(milliseconds, &interval);
	if (rc != 0) {
		return rc;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		return -errno;
	}
	value.tv_nsec = now.tv_nsec + interval.tv_nsec;
	seconds = (uintmax_t)interval.tv_sec + (value.tv_nsec >= 1000000000);
	if (seconds > OSI_TIME_MAX || (uintmax_t)now.tv_sec > OSI_TIME_MAX - seconds) {
		return -EOVERFLOW;
	}
	value.tv_sec = now.tv_sec + (time_t)seconds;
	value.tv_nsec %= 1000000000;
	*deadline = value;
	return 0;
}

static int monotonic_time(uint64_t *timestamp, uint64_t units_per_second,
			  uint64_t nanoseconds_per_unit)
{
	struct timespec now;
	uint64_t fraction;

	if (!timestamp) {
		return -EINVAL;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		return -errno;
	}
	fraction = (uint64_t)now.tv_nsec / nanoseconds_per_unit;
	if (now.tv_sec < 0 ||
	    (uintmax_t)now.tv_sec > (UINT64_MAX - fraction) / units_per_second) {
		return -EOVERFLOW;
	}
	*timestamp = (uint64_t)now.tv_sec * units_per_second + fraction;
	return 0;
}

int osi_time_get_ms(uint64_t *milliseconds)
{
	return monotonic_time(milliseconds, 1000, 1000000);
}

int osi_time_get_us(uint64_t *microseconds)
{
	return monotonic_time(microseconds, 1000000, 1000);
}

int osi_sleep_ms(os_timeout_t milliseconds)
{
	struct timespec deadline;
	int rc;

	if (milliseconds < 0) {
		return -EINVAL;
	}
	if (milliseconds == 0) {
		return 0;
	}
	rc = osi_posix_deadline(milliseconds, &deadline);
	if (rc != 0) {
		return rc;
	}
	/* An absolute deadline preserves the interval across interrupted sleeps. */
	do {
		rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
	} while (rc == EINTR);
	return -rc;
}

int osi_timeout_from_us(int64_t microseconds, os_timeout_t *milliseconds)
{
	if (!milliseconds || microseconds < 0) {
		return -EINVAL;
	}
	/* Divide before rounding so INT64_MAX does not overflow an intermediate sum. */
	*milliseconds = microseconds / 1000 + (microseconds % 1000 != 0);
	return 0;
}

int osi_timeout_from_seconds(int64_t seconds, os_timeout_t *milliseconds)
{
	if (!milliseconds || seconds < 0) {
		return -EINVAL;
	}
	if (seconds > INT64_MAX / 1000) {
		return -EOVERFLOW;
	}
	*milliseconds = seconds * 1000;
	return 0;
}
