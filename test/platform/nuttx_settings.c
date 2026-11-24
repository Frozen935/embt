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


/** @file @brief Native settings persistence check across application and simulator exits. */
#include <embt/settings/settings.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char value[] = "EmBT native settings";

static int read_value(const char *key, size_t len, settings_read_cb read_cb,
		     void *cb_arg, void *param)
{
	char loaded[sizeof(value)];
	bool *seen = param;

	if (strcmp(key, "validation")) {
		return 0;
	}
	if (len != sizeof(value) || read_cb(cb_arg, loaded, sizeof(loaded)) != sizeof(loaded) ||
	    memcmp(loaded, value, sizeof(value))) {
		return -EIO;
	}
	*seen = true;
	return 0;
}

int main(int argc, char **argv)
{
	bool seen = false;
	int err;

	if (argc != 2 || (strcmp(argv[1], "save") && strcmp(argv[1], "load") &&
			  strcmp(argv[1], "delete"))) {
		fputs("Usage: embt_settings_check save|load|delete\n", stderr);
		return 2;
	}
	err = settings_subsys_init();
	if (!err && !strcmp(argv[1], "save")) {
		err = settings_save_one("port/validation", value, sizeof(value));
	} else if (!err && !strcmp(argv[1], "delete")) {
		err = settings_delete("port/validation");
	}
	if (!err) {
		err = settings_load_subtree_direct("port", read_value, &seen);
	}
	if (!err && seen != (strcmp(argv[1], "delete") != 0)) {
		err = -ENOENT;
	}
	if (err) {
		fprintf(stderr, "Native settings %s failed (%d)\n", argv[1], err);
		return 1;
	}
	printf("Native settings %s passed\n", argv[1]);
	return 0;
}
