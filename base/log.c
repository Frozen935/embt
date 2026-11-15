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
 * @brief Log admission, runtime filtering and bounded hexadecimal rows.
 */

#include <embt/base/log.h>
#include <embt/base/atomic.h>
#include <embt/osi/io.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <embt/osi/time.h>

#ifdef CONFIG_EMBT_LOG
static bt_atomic_t runtime_level = LOG_LEVEL_DBG;
#else
static bt_atomic_t runtime_level = LOG_LEVEL_NONE;
#endif

bool bt_log_level_check(int level)
{
#ifdef CONFIG_EMBT_LOG
	return level >= LOG_LEVEL_ERR && level <= LOG_LEVEL_DBG &&
	       level <= bt_atomic_get(&runtime_level);
#else
	(void)level;
	return false;
#endif
}

int bt_log_set_level(int level)
{
	if (level < LOG_LEVEL_NONE || level > LOG_LEVEL_DBG) {
		return -EINVAL;
	}
#ifdef CONFIG_EMBT_LOG
	bt_atomic_set(&runtime_level, level);
#endif
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

void bt_log_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	osi_vprint(fmt, args);
	va_end(args);
}

void bt_log_hexdump(int level, const void *data, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	const unsigned char *bytes = data;
	char row[2 + 2 * sizeof(size_t) + 1 + 16 * 3 + 1];

	if (!bt_log_level_check(level) || (!data && len)) {
		return;
	}
	for (size_t offset = 0; offset < len;) {
		size_t count = len - offset < 16 ? len - offset : 16;
		int prefix = snprintf(row, sizeof(row), "  %04zx:", offset);
		size_t used;

		if (prefix < 0 || (size_t)prefix >= sizeof(row)) {
			return;
		}
		used = (size_t)prefix;
		for (size_t i = 0; i < count; i++) {
			unsigned char byte = bytes[offset + i];

			row[used++] = ' ';
			row[used++] = hex[byte >> 4];
			row[used++] = hex[byte & 0xf];
		}
		row[used] = '\0';
		bt_log_print("%s\n", row);
		offset += count;
	}
}
