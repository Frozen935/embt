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
 * @brief Hosted formatting and fatal assertions independent of the retired logger.
 */

#include <base/base.h>
#include <embt/sys/printk.h>
#include <stdarg.h>
#include <stdio.h>

static void output(const char *fmt, va_list args)
{
	char local[256];
	char *buffer = local;
	va_list copy;
	int size;

	va_copy(copy, args);
	size = vsnprintf(local, sizeof(local), fmt, copy);
	va_end(copy);
	if (size < 0) {
		return;
	}
	if ((size_t)size >= sizeof(local)) {
		buffer = osi_malloc((size_t)size + 1);
		if (!buffer) {
			bt_base_require(osi_console_write(local, sizeof(local) - 1, NULL));
			return;
		}
		vsnprintf(buffer, (size_t)size + 1, fmt, args);
	}
	bt_base_require(osi_console_write(buffer, (size_t)size, NULL));
	if (buffer != local) {
		osi_free(buffer);
	}
}

int vsnprintk(char *str, size_t size, const char *fmt, va_list args)
{
	return vsnprintf(str, size, fmt, args);
}

int snprintk(char *str, size_t size, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = vsnprintk(str, size, fmt, args);
	va_end(args);
	return ret;
}

void vprintk(const char *fmt, va_list args)
{
	output(fmt, args);
}

void printk(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	output(fmt, args);
	va_end(args);
}

void vprintk_unlocked(const char *fmt, va_list args)
{
	output(fmt, args);
}

void printk_unlocked(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	output(fmt, args);
	va_end(args);
}

void printk_panic(void)
{
	/* Output does not acquire a base lock and remains usable during fatal handling. */
}

void zassert_vprint(const char *fmt, va_list args)
{
	output(fmt, args);
}

void zassert_post_action(const char *file, unsigned int line)
{
	(void)file;
	(void)line;
	osi_fatal(NULL);
}

void zassert_fail(const char *condition, const char *file, unsigned int line, const char *fmt, ...)
{
	printk("ASSERTION FAIL [%s] @ %s:%u\n", condition ? condition : "", file ? file : "", line);
	if (fmt) {
		va_list args;

		va_start(args, fmt);
		output(fmt, args);
		va_end(args);
		printk("\n");
	}
	zassert_post_action(file, line);
}
