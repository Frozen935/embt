/* Copyright (c) 2019 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <embt/sys/util.h>

#include <embt/settings/settings.h>
#include "settings_priv.h"

struct read_cb_arg {
	const void *data;
	size_t len;
};

static ssize_t settings_runtime_read_cb(void *cb_arg, void *data, size_t len)
{
	struct read_cb_arg *arg = (struct read_cb_arg *)cb_arg;

	if (!len || !arg->len) {
		return 0;
	}
	memcpy(data, arg->data, MIN(arg->len, len));
	return MIN(arg->len, len);
}

int settings_runtime_set(const char *name, const void *data, size_t len)
{
	struct settings_handler_static *ch;
	const char *name_key;
	struct read_cb_arg arg;

	if (!name || (len && !data)) {
		return -EINVAL;
	}
	settings_lock_take();
	ch = settings_parse_and_lookup(name, &name_key);
	if (!ch) {
		settings_lock_release();
		return -EINVAL;
	}

	if (!ch->h_set) {
		settings_lock_release();
		return -ENOTSUP;
	}

	arg.data = data;
	arg.len = len;
	int result = ch->h_set(name_key, len, settings_runtime_read_cb, (void *)&arg);

	settings_lock_release();
	return result;
}

int settings_runtime_get(const char *name, void *data, size_t len)
{
	struct settings_handler_static *ch;
	const char *name_key;

	if (!name || (len && !data)) {
		return -EINVAL;
	}
	if (len > INT_MAX) {
		return -EOVERFLOW;
	}
	settings_lock_take();
	ch = settings_parse_and_lookup(name, &name_key);
	if (!ch) {
		settings_lock_release();
		return -EINVAL;
	}

	if (!ch->h_get) {
		settings_lock_release();
		return -ENOTSUP;
	}

	int result = ch->h_get(name_key, data, len);

	settings_lock_release();
	return result;
}

int settings_runtime_commit(const char *name)
{
	struct settings_handler_static *ch;
	const char *name_key;

	settings_lock_take();
	ch = settings_parse_and_lookup(name, &name_key);
	if (!ch) {
		settings_lock_release();
		return -EINVAL;
	}

	int result = ch->h_commit ? ch->h_commit() : 0;

	settings_lock_release();
	return result;
}
