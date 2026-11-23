/** @file
 *  @brief Immediate Alert Service implementation
 */

/*
 * Copyright (c) 2022 Codecoup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <embt/base/assert.h>
#include <errno.h>
#include <stdint.h>
#include <embt/net_buf.h>
#include <embt/base/log.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/services/ias.h>
LOG_MODULE_REGISTER(ias, CONFIG_BT_IAS_LOG_LEVEL);

static struct bt_ias_cb *callbacks;

#define BT_IAS_ALERT_LVL_LEN 1

#if defined(CONFIG_BT_IAS_SEC_AUTH)
#define IAS_ALERT_LEVEL_PERM BT_GATT_PERM_WRITE_AUTHEN
#elif defined(CONFIG_BT_IAS_SEC_ENC)
#define IAS_ALERT_LEVEL_PERM BT_GATT_PERM_WRITE_ENCRYPT
#else
#define IAS_ALERT_LEVEL_PERM BT_GATT_PERM_WRITE
#endif

struct alerting_device {
	enum bt_ias_alert_lvl alert_level;
};

static struct alerting_device devices[CONFIG_BT_MAX_CONN];
static enum bt_ias_alert_lvl curr_lvl;

static void set_alert_level(void)
{
	enum bt_ias_alert_lvl alert_level = BT_IAS_ALERT_LVL_NO_ALERT;

	/* Set the alert_level as the highest requested by any device */
	for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if (alert_level < devices[i].alert_level) {
			alert_level = devices[i].alert_level;
		}
	}

	if (curr_lvl == alert_level) {
		return;
	}

	curr_lvl = alert_level;

	if (alert_level == BT_IAS_ALERT_LVL_HIGH_ALERT) {
		for (struct bt_ias_cb *cb = callbacks; cb; cb = cb->next) {
			if (cb->high_alert) {
				cb->high_alert();
			}
		}
		LOG_DBG("High alert");
	} else if (alert_level == BT_IAS_ALERT_LVL_MILD_ALERT) {
		for (struct bt_ias_cb *cb = callbacks; cb; cb = cb->next) {
			if (cb->mild_alert) {
				cb->mild_alert();
			}
		}
		LOG_DBG("Mild alert");
	} else {
		for (struct bt_ias_cb *cb = callbacks; cb; cb = cb->next) {
			if (cb->no_alert) {
				cb->no_alert();
			}
		}
		LOG_DBG("No alert");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	devices[bt_conn_index(conn)].alert_level = BT_IAS_ALERT_LVL_NO_ALERT;
	set_alert_level();
}

int bt_ias_local_alert_stop(void)
{
	if (curr_lvl == BT_IAS_ALERT_LVL_NO_ALERT) {
		return -EALREADY;
	}

	for (int idx = 0; idx < CONFIG_BT_MAX_CONN; idx++) {
		devices[idx].alert_level = BT_IAS_ALERT_LVL_NO_ALERT;
	}
	curr_lvl = BT_IAS_ALERT_LVL_NO_ALERT;
	set_alert_level();

	return 0;
}

static ssize_t bt_ias_write_alert_lvl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				      const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	struct net_buf_simple data;
	enum bt_ias_alert_lvl alert_val;

	if (offset > 0) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (len != BT_IAS_ALERT_LVL_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	net_buf_simple_init_with_data(&data, (void *)buf, len);
	alert_val = net_buf_simple_pull_u8(&data);
	devices[bt_conn_index(conn)].alert_level = alert_val;

	if (alert_val < BT_IAS_ALERT_LVL_NO_ALERT || alert_val > BT_IAS_ALERT_LVL_HIGH_ALERT) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	set_alert_level();

	return len;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.disconnected = disconnected,
};

/* Immediate Alert Service Declaration */
BT_GATT_SERVICE_DEFINE(ias_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_IAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_ALERT_LEVEL, BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       IAS_ALERT_LEVEL_PERM, NULL,
			       bt_ias_write_alert_lvl, NULL));

int bt_service_ias_ias_register(void)
{
	int err;

	err = bt_conn_cb_register(&bt_conn_cb_conn_callbacks);
	if (err && err != -EEXIST) {
		return err;
	}
	err = bt_gatt_service_static_register(&ias_svc);
	if (err && err != -EEXIST) {
		return err;
	}
	return 0;
}

int bt_ias_cb_register(struct bt_ias_cb *cb)
{
	if (!cb) {
		return -EINVAL;
	}
	for (struct bt_ias_cb *entry = callbacks; entry; entry = entry->next) {
		if (entry == cb) {
			return -EALREADY;
		}
	}
	cb->next = callbacks;
	callbacks = cb;
	return 0;
}
