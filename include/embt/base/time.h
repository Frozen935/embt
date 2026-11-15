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
 * @brief Monotonic deadlines for internal mechanisms.
 */

#ifndef EMBT_BASE_TIME_H_
#define EMBT_BASE_TIME_H_

#include <stdbool.h>
#include <stdint.h>

/** @brief Relative milliseconds; -1 means forever and zero means no wait. */
typedef int64_t bt_timeout_t;
/** @brief Absolute monotonic deadline in milliseconds. */
typedef struct {
	int64_t deadline_ms;
} bt_timepoint_t;
#define BT_MSEC(milliseconds) ((bt_timeout_t)(milliseconds))
#define BT_SECONDS(seconds) BT_MSEC(INT64_C(1000) * (seconds))
#define BT_MINUTES(minutes) BT_SECONDS(INT64_C(60) * (minutes))
#define BT_NO_WAIT BT_MSEC(0)
#define BT_FOREVER BT_MSEC(-1)
#define BT_TIMEOUT_EQ(first, second) ((first) == (second))

#define MSEC_PER_SEC 1000U
#define USEC_PER_SEC 1000000U
#define NSEC_PER_SEC 1000000000U
#define USEC_PER_MSEC 1000U
#define NSEC_PER_MSEC 1000000U
#define NSEC_PER_USEC 1000U

/** @brief Unlimited duration for interfaces whose duration is in milliseconds. */
#define SYS_FOREVER_MS (-1)

/** @brief Convert a millisecond duration into the owning timeout representation. */
#define SYS_TIMEOUT_MS(milliseconds) BT_MSEC(milliseconds)

/**
 * @brief Round a nonnegative microsecond interval up to milliseconds.
 * @param microseconds Relative microseconds.
 * @return Milliseconds; zero remains zero, and positive fractions become one.
 *
 * Division before rounding avoids an intermediate addition overflow. Even the
 * largest uint64_t input produces a result representable by int64_t.
 */
static inline uint64_t bt_time_us_to_ms_ceil(uint64_t microseconds)
{
	return microseconds / USEC_PER_MSEC + (microseconds % USEC_PER_MSEC != 0);
}


/**
 * @brief Read monotonic uptime in milliseconds.
 * @return Monotonic milliseconds.
 */
int64_t bt_uptime_get_ms(void);

/**
 * @brief Read monotonic microseconds for protocol timing and statistics.
 * @return Monotonic microseconds from the OSI clock origin.
 * @note A backend failure is fatal, matching the existing uptime contract.
 */
uint64_t bt_uptime_get_us(void);

/**
 * @brief Read the low 32 bits of monotonic uptime in milliseconds.
 *
 * The result wraps modulo 2^32.
 * @return Monotonic milliseconds modulo 2^32.
 */
uint32_t bt_uptime_get_ms_32(void);
/**
 * @brief Convert a relative wait into one absolute monotonic deadline.
 *
 * Saturates to the forever deadline if addition would overflow.
 *
 * @param[in] timeout Relative milliseconds; BT_NO_WAIT or BT_FOREVER when supported.
 * @return Absolute deadline, including the forever sentinel.
 */
bt_timepoint_t bt_timepoint_calc(bt_timeout_t timeout);
/**
 * @brief Compute the remaining wait without restarting its deadline.
 *
 * An expired deadline returns BT_NO_WAIT.
 *
 * @param[in] end Absolute monotonic deadline.
 * @return Remaining relative timeout.
 */
bt_timeout_t bt_timepoint_timeout(bt_timepoint_t end);
/**
 * @brief Test whether a finite monotonic deadline has expired.
 *
 * @param[in] end Absolute monotonic deadline.
 * @return True if the condition holds, otherwise false.
 */
bool bt_timepoint_expired(bt_timepoint_t end);

#endif /* EMBT_BASE_TIME_H_ */
