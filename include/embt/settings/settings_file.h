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

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SETTINGS_FILE_H_
#define __SETTINGS_FILE_H_

#include <embt/compiler.h>
#include <limits.h>
#include <embt/settings/settings.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_FILE_NAME_MAX PATH_MAX /* max length for settings filename */

struct settings_file {
	struct settings_store cf_store;
	const char *cf_name; /* filename */
	int cf_maxlines;     /* max # of lines before compressing */
	int cf_lines;        /* private */
};

/**
 * @brief Register a persistent file source using the original settings store interface.
 * @param[in,out] cf Process-lifetime descriptor with a live pathname.
 * @return Zero or a negative errno for invalid configuration.
 * @pre Initialize the settings subsystem and serialize backend registration.
 */
int settings_file_src(struct settings_file *cf);

/**
 * @brief Select a file destination for settings_save_one and handler exports.
 * @param[in,out] cf Process-lifetime descriptor with a live pathname.
 * @return Zero or a negative errno for invalid configuration.
 * @note New files use owner-only permissions; callbacks may read through read_cb.
 *   Recursive backend operations from load callbacks return -EDEADLK.
 */
int settings_file_dst(struct settings_file *cf);

/**
 * @brief Install the file provider's original line read/write callbacks.
 * @param[in] cf Registered file descriptor; provider format is shared by file stores.
 * @pre Serialize this initialization before any settings file operation.
 */
void settings_mount_file_backend(struct settings_file *cf);

/**
 * @brief Select the default backend path before settings_subsys_init().
 * @param[in] path Process-lifetime pathname; borrowed, not copied.
 * @return Zero, -EINVAL for an invalid path or -EBUSY after initialization.
 * @pre Serialize subsystem lifecycle calls. The pathname outlives all settings APIs.
 */
int settings_file_set_path(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_FILE_H_ */
