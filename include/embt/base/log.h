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
#include <embt/base/atomic.h>

#define LOG_LEVEL_NONE 0
#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_INF 3
#define LOG_LEVEL_DBG 4

/**
 * @brief Resolve a module level, including inherited defaults and the master switch.
 * @param level Configured level: -1 inherits, 0 disables, and 1..4 emit logs.
 *
 * This expression also works in preprocessor conditions. LOG_LEVEL is a C
 * constant created by the module declaration and must not be used in #if.
 */
#ifdef CONFIG_EMBT_LOG
#define BT_LOG_LEVEL_RESOLVE(level) ((level) == -1 ? CONFIG_EMBT_LOG_LEVEL : (level))
#else
#define BT_LOG_LEVEL_RESOLVE(level) 0
#endif

/**
 * @brief Check whether a module's compiled configuration admits a log level.
 * @param module Configuration prefix, such as BT_CONN or BT_MESH_MODEL.
 * @param level Level suffix: ERR, WRN, INF, or DBG.
 *
 * Works in both C expressions and #if, including inherited defaults and the
 * master switch. This predicate does not consult the runtime output filter.
 */
#define BT_LOG_LEVEL_ENABLED(module, level) \
	(BT_LOG_LEVEL_RESOLVE(CONFIG_##module##_LOG_LEVEL) >= LOG_LEVEL_##level)

#define LOG_TAG bt_log_module_tag
#define LOG_LEVEL bt_log_module_level

/** @brief Define file-local module metadata without startup registration. */
#define BT_LOG_MODULE_DEFINE(name, level)                                                   \
	static const char bt_log_module_tag[] __attribute__((unused)) = #name;              \
	enum { bt_log_module_level = BT_LOG_LEVEL_RESOLVE(level) }

#define BT_LOG_MODULE_DEFAULT(name) BT_LOG_MODULE_DEFINE(name, -1)
#define BT_LOG_MODULE_SELECT(_1, _2, selected, ...) selected

/**
 * @brief Declare the module tag and compiled level before its first log use.
 * @param name Module name, converted to its output tag.
 * @param ... Optional level; omit to inherit CONFIG_EMBT_LOG_LEVEL.
 *
 * Use once per source file. Metadata is constant and private to this translation
 * unit; there is no registry, dynamic allocation, or initialization callback.
 */
#define LOG_MODULE_REGISTER(...)                                                           \
	BT_LOG_MODULE_SELECT(__VA_ARGS__, BT_LOG_MODULE_DEFINE, BT_LOG_MODULE_DEFAULT,       \
			     unused)(__VA_ARGS__)

/**
 * @brief Use the same tag and configuration as a module in another source file.
 * @param name Shared module name.
 * @param ... The same configured level as the owning source file.
 *
 * This creates file-local metadata, not an external mutable module descriptor.
 */
#define LOG_MODULE_DECLARE(...) LOG_MODULE_REGISTER(__VA_ARGS__)

/** @brief Check this module's compiled level before the global runtime filter. */
#define BT_LOG_ENABLED(level) (LOG_LEVEL >= (level) && bt_log_level_check(level))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check the master switch and global runtime filter only.
 * @param[in] level Message level from LOG_LEVEL_ERR to LOG_LEVEL_DBG.
 * @return True when admitted globally; callers must also check the module level.
 */
bool bt_log_level_check(int level);

/**
 * @brief Set the global runtime filter without changing compiled module levels.
 *
 * Safe to call concurrently with logging. No initialization is required.
 * @param[in] level Level from LOG_LEVEL_NONE to LOG_LEVEL_DBG.
 * @return Zero on success, otherwise -EINVAL.
 */
int bt_log_set_level(int level);

/**
 * @brief Read the current runtime level.
 * @return The current level, initially Debug when enabled, otherwise Off.
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
 * @brief Adapt variadic text to osi_vprint, without filtering or level arguments.
 * @param[in] fmt Printf format, followed by arguments; includes its own newline.
 * @note LOG macros perform filtering before this call; assertion reporting may
 * call it directly. This function does not allocate message memory.
 */
void bt_log_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Emit bytes as independently serialized rows of at most sixteen bytes.
 * @param[in] level Message level used only by base filtering.
 * @param[in] data Bytes borrowed until return; may be NULL if len is zero.
 * @param[in] len Byte count.
 * @note LOG_HEXDUMP_* emits the label. Other messages can occur between rows.
 */
void bt_log_hexdump(int level, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

/** @brief Filter first and construct one complete literal-format record. */
#define BT_LOG(level, fmt, ...)                                                       \
	do {                                                                         \
		if (BT_LOG_ENABLED(level)) {                                          \
			bt_log_print("[%c][%s:%u] %s: " fmt "\n",                   \
				"NEWID"[level], LOG_TAG, (unsigned int)__LINE__, __func__,            \
				##__VA_ARGS__);                                      \
		}                                                                    \
	} while (0)

#define LOG_ERR(...) BT_LOG(LOG_LEVEL_ERR, __VA_ARGS__)
#define LOG_WRN(...) BT_LOG(LOG_LEVEL_WRN, __VA_ARGS__)
#define LOG_INF(...) BT_LOG(LOG_LEVEL_INF, __VA_ARGS__)
#define LOG_DBG(...) BT_LOG(LOG_LEVEL_DBG, __VA_ARGS__)

/** @brief Filter before evaluating the literal label or byte arguments. */
#define BT_LOG_HEXDUMP(level, data, len, fmt, ...)                                     \
	do {                                                                         \
		if (BT_LOG_ENABLED(level)) {                                          \
			const void *bt_dump_data = (data);                            \
			size_t bt_dump_len = (len);                                   \
			if (bt_dump_data || !bt_dump_len) {                           \
				BT_LOG(level, fmt, ##__VA_ARGS__);                     \
				bt_log_hexdump(level, bt_dump_data, bt_dump_len);       \
			}                                                            \
		}                                                                    \
	} while (0)

#define LOG_HEXDUMP_ERR(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_ERR, data, len, __VA_ARGS__)
#define LOG_HEXDUMP_WRN(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_WRN, data, len, __VA_ARGS__)
#define LOG_HEXDUMP_INF(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_INF, data, len, __VA_ARGS__)
#define LOG_HEXDUMP_DBG(data, len, ...) BT_LOG_HEXDUMP(LOG_LEVEL_DBG, data, len, __VA_ARGS__)

/** @brief Rate-limit at the call site before evaluating message arguments. */
#define BT_LOG_RATELIMIT(level, interval, fmt, ...)                                    \
	do {                                                                         \
		static bt_atomic_t last;                                              \
		if (BT_LOG_ENABLED(level) && bt_log_ratelimit(&last, interval)) {       \
			BT_LOG(level, fmt, ##__VA_ARGS__);                             \
		}                                                                    \
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
