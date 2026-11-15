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
 * @brief Internal LOG interfaces backed by synchronous hosted output.
 */

#ifndef EMBT_BASE_LOG_H_
#define EMBT_BASE_LOG_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <base/bt_atomic.h>

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_INF 3
#define LOG_LEVEL_DBG 4

#ifndef LOG_TAG
#define LOG_TAG __FILE__
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check the runtime level and the configured maximum.
 * @param[in] level Message level from LOG_LEVEL_ERR to LOG_LEVEL_DBG.
 * @return True when the message can be emitted.
 */
bool bt_log_level_check(int level);

/**
 * @brief Set the runtime level within the configured maximum.
 *
 * Safe to call concurrently with logging. No initialization is required.
 * @param[in] level Level from LOG_LEVEL_NONE to CONFIG_EMBT_LOG_LEVEL.
 * @return Zero on success, otherwise -EINVAL.
 */
int bt_log_set_level(int level);

/**
 * @brief Read the current runtime level.
 * @return The current level, initially CONFIG_EMBT_LOG_LEVEL.
 */
int bt_log_get_level(void);

/**
 * @brief Admit the first message and at most one per subsequent interval.
 * @param[in,out] last Zero-initialized call-site state shared by all threads.
 * @param[in] interval Interval in milliseconds, at most INT32_MAX.
 * @return True when this call wins admission; time errors suppress admission.
 */
bool bt_log_ratelimit(bt_atomic_t *last, uint32_t interval);

/**
 * @brief Emit one complete formatted message to stderr.
 *
 * Output is synchronous, serialized through stdio, and not signal safe.
 * @param[in] level Message level.
 * @param[in] tag Module tag.
 * @param[in] func Calling function.
 * @param[in] line Source line.
 * @param[in] fmt Printf format followed by its arguments.
 */
void bt_log_print(int level, const char *tag, const char *func, unsigned int line,
		  const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/**
 * @brief Emit a labeled hexadecimal dump under the same level filter.
 *
 * The complete dump is serialized with other log messages. A zero length
 * allows NULL data. The data remains borrowed until this call returns.
 * @param[in] level Message level.
 * @param[in] tag Module tag.
 * @param[in] func Calling function.
 * @param[in] line Source line.
 * @param[in] data Bytes to print.
 * @param[in] len Number of bytes.
 * @param[in] fmt Printf label followed by its arguments.
 */
void bt_log_hexdump(int level, const char *tag, const char *func, unsigned int line,
		    const void *data, size_t len, const char *fmt, ...)
	__attribute__((format(printf, 7, 8)));

#ifdef __cplusplus
}
#endif

/** @brief Filter before evaluating message arguments. */
#define BT_LOG(level, ...)                                                                  \
	do {                                                                               \
		if (CONFIG_EMBT_LOG_LEVEL >= (level) && bt_log_level_check(level)) {          \
			bt_log_print(level, LOG_TAG, __func__, __LINE__, __VA_ARGS__);        \
		}                                                                          \
	} while (0)

#define LOG_ERR(...) BT_LOG(LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_WRN(...) BT_LOG(LOG_LEVEL_WRN, __VA_ARGS__)
#define LOG_INF(...) BT_LOG(LOG_LEVEL_INF, __VA_ARGS__)
#define LOG_DBG(...) BT_LOG(LOG_LEVEL_DBG, __VA_ARGS__)

/** @brief Filter both the label and the bytes before evaluating arguments. */
#define BT_LOG_HEXDUMP(level, data, len, ...)                                                \
	do {                                                                               \
		if (CONFIG_EMBT_LOG_LEVEL >= (level) && bt_log_level_check(level)) {          \
			bt_log_hexdump(level, LOG_TAG, __func__, __LINE__, data, len,          \
				       __VA_ARGS__);                                                \
		}                                                                          \
	} while (0)

#define LOG_HEXDUMP_ERR(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_ERR, data, len, __VA_ARGS__)
#define LOG_HEXDUMP_WRN(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_WRN, data, len, __VA_ARGS__)
#define LOG_HEXDUMP_INF(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_INF, data, len, __VA_ARGS__)
#define LOG_HEXDUMP_DBG(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_DBG, data, len, __VA_ARGS__)

/** @brief Apply a per-call-site interval before evaluating message arguments. */
#define BT_LOG_RATELIMIT(level, interval, ...)                                              \
	do {                                                                               \
		static bt_atomic_t last;                                                    \
		if (CONFIG_EMBT_LOG_LEVEL >= (level) && bt_log_level_check(level) &&          \
		    bt_log_ratelimit(&last, interval)) {                                     \
			bt_log_print(level, LOG_TAG, __func__, __LINE__, __VA_ARGS__);        \
		}                                                                          \
	} while (0)

#define LOG_ERR_RATELIMIT_RATE(ms, ...) BT_LOG_RATELIMIT(LOG_LEVEL_ERR, ms, __VA_ARGS__)
#define LOG_WRN_RATELIMIT_RATE(ms, ...) BT_LOG_RATELIMIT(LOG_LEVEL_WRN, ms, __VA_ARGS__)
#define LOG_INF_RATELIMIT_RATE(ms, ...) BT_LOG_RATELIMIT(LOG_LEVEL_INF, ms, __VA_ARGS__)
#define LOG_DBG_RATELIMIT_RATE(ms, ...) BT_LOG_RATELIMIT(LOG_LEVEL_DBG, ms, __VA_ARGS__)
#define LOG_ERR_RATELIMIT(...) LOG_ERR_RATELIMIT_RATE(1000, __VA_ARGS__)
#define LOG_WRN_RATELIMIT(...) LOG_WRN_RATELIMIT_RATE(1000, __VA_ARGS__)
#define LOG_INF_RATELIMIT(...) LOG_INF_RATELIMIT_RATE(1000, __VA_ARGS__)
#define LOG_DBG_RATELIMIT(...) LOG_DBG_RATELIMIT_RATE(1000, __VA_ARGS__)

#endif /* EMBT_BASE_LOG_H_ */
