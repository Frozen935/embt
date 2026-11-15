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
 * @brief Synchronous logging without Zephyr registration or message queues.
 */

#include <base/log.h>
#include <base/bt_atomic.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <osi/time.h>

static bt_atomic_t runtime_level = CONFIG_EMBT_LOG_LEVEL;

bool bt_log_level_check(int level)
{
	return level >= LOG_LEVEL_ERR && level <= CONFIG_EMBT_LOG_LEVEL &&
	       level <= bt_atomic_get(&runtime_level);
}

int bt_log_set_level(int level)
{
	if (level < LOG_LEVEL_NONE || level > CONFIG_EMBT_LOG_LEVEL) {
		return -EINVAL;
	}
	bt_atomic_set(&runtime_level, level);
	return 0;
}

int bt_log_get_level(void)
{
	return (int)bt_atomic_get(&runtime_level);
}

bool bt_log_ratelimit(bt_atomic_t *last, uint32_t interval)
{
	uint64_t now;
	bt_atomic_val_t previous;
	uint32_t stamp;

	if (interval > INT32_MAX || osi_time_get_ms(&now)) {
		return false;
	}
	stamp = (uint32_t)now;
	previous = bt_atomic_get(last);
	for (;;) {
		if (previous && (uint32_t)(stamp - ((uint32_t)previous - 1)) < interval) {
			return false;
		}
		if (bt_atomic_cas(last, previous, (bt_atomic_val_t)(uint32_t)(stamp + 1))) {
			return true;
		}
		previous = bt_atomic_get(last);
	}
}

static void prefix(int level, const char *tag, const char *func, unsigned int line)
{
	static const char levels[] = "NEWID";

	fprintf(stderr, "[%c][%s:%u] %s: ", levels[level], tag, line, func);
}

void bt_log_print(int level, const char *tag, const char *func, unsigned int line,
		  const char *fmt, ...)
{
	va_list args;

	if (!bt_log_level_check(level)) {
		return;
	}
	flockfile(stderr);
	prefix(level, tag, func, line);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	funlockfile(stderr);
}

void bt_log_hexdump(int level, const char *tag, const char *func, unsigned int line,
		    const void *data, size_t len, const char *fmt, ...)
{
	const uint8_t *bytes = data;
	va_list args;

	if (!bt_log_level_check(level) || (!data && len)) {
		return;
	}
	flockfile(stderr);
	prefix(level, tag, func, line);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	for (size_t off = 0; off < len;) {
		size_t count = len - off < 16 ? len - off : 16;

		fprintf(stderr, "  %04zx:", off);
		for (size_t i = 0; i < count; i++) {
			fprintf(stderr, " %02x", bytes[off + i]);
		}
		fputc('\n', stderr);
		off += count;
	}
	funlockfile(stderr);
}
