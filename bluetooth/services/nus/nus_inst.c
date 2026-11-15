/*
 * Copyright (c) 2024 Croxel, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nus_internal.h"
#include <errno.h>

static struct bt_nus_inst *instances;

#if defined(CONFIG_BT_ZEPHYR_NUS_DEFAULT_INSTANCE)
BT_NUS_INST_DEFINE(nus_def);
struct bt_nus_inst *bt_nus_inst_default(void)
{
	return &nus_def;
}
#else
struct bt_nus_inst *bt_nus_inst_default(void) { return NULL; }
#endif

struct bt_nus_inst *bt_nus_inst_get_from_attr(const struct bt_gatt_attr *attr)
{
	for (struct bt_nus_inst *instance = instances; instance; instance = instance->next) {
		for (size_t i = 0 ; i < instance->svc->attr_count ; i++) {
			if (attr == &instance->svc->attrs[i]) {
				return instance;
			}
		}
	}

	return NULL;
}

int bt_nus_inst_register(struct bt_nus_inst *instance)
{
	int err;

	if (!instance || !instance->svc || !instance->cbs) {
		return -EINVAL;
	}
	for (struct bt_nus_inst *entry = instances; entry; entry = entry->next) {
		if (entry == instance) {
			return -EALREADY;
		}
	}
	err = bt_gatt_service_static_register((struct bt_gatt_service_static *)instance->svc);
	if (err) {
		return err;
	}
	instance->next = instances;
	instances = instance;
	return 0;
}

int bt_nus_base_init(void)
{
#if defined(CONFIG_BT_ZEPHYR_NUS_DEFAULT_INSTANCE)
	return bt_nus_inst_register(&nus_def);
#else
	return 0;
#endif
}
