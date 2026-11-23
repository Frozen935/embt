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
 * @brief POSIX raw stderr output and abort-based fatal termination.
 */

#include <embt/osi/io.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef __NuttX__
#include <syslog.h>
#else
#include <stdio.h>
#endif

void osi_vprint(const char *fmt, va_list args)
{
#ifdef __NuttX__
	vsyslog(LOG_INFO, fmt, args);
#else
	(void)vfprintf(stderr, fmt, args);
#endif
}

int osi_console_write(const void *data, size_t length, size_t *written)
{
	const unsigned char *bytes = data;
	ssize_t count;

	if (!written) {
		return -EINVAL;
	}
	*written = 0;
	if (!data && length) {
		return -EINVAL;
	}
	while (*written < length) {
		count = write(STDERR_FILENO, bytes + *written, length - *written);
		if (count < 0) {
			if (errno == EINTR) {
				continue;
			}
			return -errno;
		}
		if (count == 0) {
			return -EIO;
		}
		*written += (size_t)count;
	}
	return 0;
}

OS_NORETURN void osi_fatal(const char *message)
{
	size_t written;

	if (message) {
		osi_console_write(message, strlen(message), &written);
		osi_console_write("\n", 1, &written);
	}
	abort();
}
