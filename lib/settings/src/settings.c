/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2015 Runtime Inc
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <embt/base/base.h>
#include <embt/base/atomic.h>
#include <embt/osi/sync.h>
#include <embt/osi/thread.h>
#include <embt/osi/memory.h>
#include <embt/base/assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

#include <embt/settings/settings.h>
#include "settings_priv.h"
#include <stddef.h>
#include <stdint.h>
#include <embt/base/log.h>

LOG_MODULE_REGISTER(settings, CONFIG_SETTINGS_LOG_LEVEL);

#if defined(CONFIG_SETTINGS_DYNAMIC_HANDLERS)
sys_slist_t settings_handlers;
#endif /* CONFIG_SETTINGS_DYNAMIC_HANDLERS */

static os_mutex_t settings_lock;
static bt_atomic_t settings_lock_state;
sys_slist_t settings_static_handlers;

int settings_ensure_lock(void)
{
	for (;;) {
		if (bt_atomic_get(&settings_lock_state) == 2) {
			return 0;
		}
		if (bt_atomic_cas(&settings_lock_state, 0, 1)) {
			int err = osi_mutex_init(&settings_lock, OS_MUTEX_RECURSIVE);

			bt_atomic_set(&settings_lock_state, err ? 0 : 2);
			return err;
		}
		osi_thread_yield();
	}
}

void settings_store_init(void);

void settings_init(void)
{
	sys_slist_init(&settings_static_handlers);
#if defined(CONFIG_SETTINGS_DYNAMIC_HANDLERS)
	sys_slist_init(&settings_handlers);
#endif /* CONFIG_SETTINGS_DYNAMIC_HANDLERS */
	settings_store_init();
}

#if defined(CONFIG_SETTINGS_DYNAMIC_HANDLERS)
int settings_register_with_cprio(struct settings_handler *handler, int cprio)
{
	int rc = 0;

	if (!handler || !handler->name || !handler->name[0]) {
		return -EINVAL;
	}
	settings_lock_take();
	SETTINGS_STATIC_FOREACH(ch)
	{
		if (strcmp(handler->name, ch->name) == 0) {
			rc = -EEXIST;
			goto end;
		}
	}

	struct settings_handler *ch;
	SYS_SLIST_FOR_EACH_CONTAINER(&settings_handlers, ch, node) {
		if (strcmp(handler->name, ch->name) == 0) {
			rc = -EEXIST;
			goto end;
		}
	}

	handler->cprio = cprio;
	sys_slist_append(&settings_handlers, &handler->node);

end:
	settings_lock_release();
	return rc;
}

int settings_register(struct settings_handler *handler)
{
	return settings_register_with_cprio(handler, 0);
}
#endif /* CONFIG_SETTINGS_DYNAMIC_HANDLERS */

int settings_name_steq(const char *name, const char *key, const char **next)
{
	if (next) {
		*next = NULL;
	}

	if ((!name) || (!key)) {
		return 0;
	}

	/* name might come from flash directly, in flash the name would end
	 * with '=' or '\0' depending how storage is done. Flash reading is
	 * limited to what can be read
	 */

	while ((*key != '\0') && (*key == *name) && (*name != '\0') &&
	       (*name != SETTINGS_NAME_END)) {
		key++;
		name++;
	}

	if (*key != '\0') {
		return 0;
	}

	if (*name == SETTINGS_NAME_SEPARATOR) {
		if (next) {
			*next = name + 1;
		}
		return 1;
	}

	if ((*name == SETTINGS_NAME_END) || (*name == '\0')) {
		return 1;
	}

	return 0;
}

int settings_name_next(const char *name, const char **next)
{
	int rc = 0;

	if (next) {
		*next = NULL;
	}

	if (!name) {
		return 0;
	}

	/* name might come from flash directly, in flash the name would end
	 * with '=' or '\0' depending how storage is done. Flash reading is
	 * limited to what can be read
	 */
	while ((*name != '\0') && (*name != SETTINGS_NAME_END) &&
	       (*name != SETTINGS_NAME_SEPARATOR)) {
		rc++;
		name++;
	}

	if (*name == SETTINGS_NAME_SEPARATOR) {
		if (next) {
			*next = name + 1;
		}
		return rc;
	}

	return rc;
}

struct settings_handler_static *settings_parse_and_lookup(const char *name,
							const char **next)
{
	struct settings_handler_static *bestmatch;
	const char *tmpnext;

	bestmatch = NULL;
	if (next) {
		*next = NULL;
	}

	SETTINGS_STATIC_FOREACH(ch)
	{
		if (!settings_name_steq(name, ch->name, &tmpnext)) {
			continue;
		}
		if (!bestmatch) {
			bestmatch = (struct settings_handler_static *)ch;
			if (next) {
				*next = tmpnext;
			}
			continue;
		}
		if (settings_name_steq(ch->name, bestmatch->name, NULL)) {
			bestmatch = (struct settings_handler_static *)ch;
			if (next) {
				*next = tmpnext;
			}
		}
	}

#if defined(CONFIG_SETTINGS_DYNAMIC_HANDLERS)
	struct settings_handler *ch;

	SYS_SLIST_FOR_EACH_CONTAINER(&settings_handlers, ch, node) {
		if (!settings_name_steq(name, ch->name, &tmpnext)) {
			continue;
		}
		if (!bestmatch) {
			bestmatch = (struct settings_handler_static *)ch;
			if (next) {
				*next = tmpnext;
			}
			continue;
		}
		if (settings_name_steq(ch->name, bestmatch->name, NULL)) {
			bestmatch = (struct settings_handler_static *)ch;
			if (next) {
				*next = tmpnext;
			}
		}
	}
#endif /* CONFIG_SETTINGS_DYNAMIC_HANDLERS */
	return bestmatch;
}

int settings_call_set_handler(const char *name,
			      size_t len,
			      settings_read_cb read_cb,
			      void *read_cb_arg,
			      const struct settings_load_arg *load_arg)
{
	int rc;
	const char *name_key = name;

	if (load_arg && load_arg->subtree &&
	    !settings_name_steq(name, load_arg->subtree, &name_key)) {
		return 0;
	}

	if (load_arg && load_arg->cb) {
		rc = load_arg->cb(name_key, len, read_cb, read_cb_arg,
				  load_arg->param);
	} else {
		struct settings_handler_static *ch;

		ch = settings_parse_and_lookup(name, &name_key);
		if (!ch) {
			return 0;
		}

		rc = ch->h_set ? ch->h_set(name_key, len, read_cb, read_cb_arg) : 0;

		if (rc != 0) {
			LOG_ERR("set-value failure. key: %s error(%d)",
				name, rc);
			/* Ignoring the error */
			rc = 0;
		} else {
			LOG_DBG("set-value OK. key: %s",
				name);
		}
	}
	return rc;
}

int settings_commit(void)
{
	return settings_commit_subtree(NULL);
}

static int set_next_cprio(int handler_cprio, int cprio, int next_cprio)
{
	if (handler_cprio <= cprio) {
		return next_cprio;
	}

	/* If cprio and next_cprio are identical then next_cprio has not
	 * yet been set to any value and its initialized to the first
	 * handler_cprio above cprio.
	 */
	if (cprio == next_cprio) {
		return handler_cprio;
	}

	return MIN(handler_cprio, next_cprio);
}

int settings_commit_subtree(const char *subtree)
{
	int rc;
	int rc2;
	int cprio = INT_MIN;

	settings_lock_take();
	rc = 0;

	while (true) {
		int next_cprio = cprio;

		SETTINGS_STATIC_FOREACH(ch)
		{
			if (subtree && !settings_name_steq(ch->name, subtree, NULL)) {
				continue;
			}

			if (ch->h_commit) {
				next_cprio = set_next_cprio(ch->cprio, cprio, next_cprio);
				if (ch->cprio != cprio) {
					continue;
				}

				rc2 = ch->h_commit();
				if (!rc) {
					rc = rc2;
				}
			}
		}

		if (IS_ENABLED(CONFIG_SETTINGS_DYNAMIC_HANDLERS)) {
			struct settings_handler *ch;

			SYS_SLIST_FOR_EACH_CONTAINER(&settings_handlers, ch, node) {
				if (subtree && !settings_name_steq(ch->name, subtree, NULL)) {
					continue;
				}

				if (ch->h_commit) {
					next_cprio = set_next_cprio(ch->cprio, cprio, next_cprio);
					if (ch->cprio != cprio) {
						continue;
					}

					rc2 = ch->h_commit();
					if (!rc) {
						rc = rc2;
					}
				}
			}
		}

		if (cprio == next_cprio) {
			break;
		}

		cprio = next_cprio;
	}

	settings_lock_release();
	return rc;
}

void settings_lock_take(void)
{
	bt_base_require(settings_ensure_lock());
	bt_base_require(osi_mutex_lock(&settings_lock, OS_TIMEOUT_FOREVER));
}

void settings_lock_release(void)
{
	bt_base_require(osi_mutex_unlock(&settings_lock));
}

int settings_register_static(const struct settings_handler_static *handler)
{
	struct settings_static_registration *entry;
	int err = 0;

	if (!handler || !handler->name || !handler->name[0]) {
		return -EINVAL;
	}
	settings_lock_take();
	SETTINGS_STATIC_FOREACH(ch)
	{
		if (!strcmp(handler->name, ch->name)) {
			err = handler == ch ? 0 : -EEXIST;
			goto out;
		}
	}
#if defined(CONFIG_SETTINGS_DYNAMIC_HANDLERS)
	struct settings_handler *dynamic;

	SYS_SLIST_FOR_EACH_CONTAINER(&settings_handlers, dynamic, node) {
		if (!strcmp(handler->name, dynamic->name)) {
			err = -EEXIST;
			goto out;
		}
	}
#endif
	entry = osi_calloc(1, sizeof(*entry));
	if (!entry) {
		err = -ENOMEM;
		goto out;
	}
	entry->handler = handler;
	sys_slist_append(&settings_static_handlers, &entry->node);
out:
	settings_lock_release();
	return err;
}
