/* hci_userchan.c - HCI user channel Bluetooth handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/base/fifo.h>
#include <embt/base/time.h>
#include <string.h>
#include <embt/base/assert.h>
#include <embt/base/util_macro.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_raw.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/l2cap.h>
#include <embt/bluetooth/iso.h>
#include <embt/drivers/bluetooth.h>
#include <embt/base/log.h>
#include <embt/net_buf.h>
#include <embt/base/atomic.h>
#include <embt/base/byteorder.h>
#include <embt/base/util_macro.h>
#include <embt/compiler.h>

#include "common/hci_common_internal.h"
#include "hci_raw_internal.h"
#include "monitor.h"

LOG_MODULE_REGISTER(bt_hci_raw, CONFIG_BT_HCI_CORE_LOG_LEVEL);

static struct bt_fifo *raw_rx;

static bt_buf_rx_freed_cb_t buf_rx_freed_cb;

static void hci_rx_buf_destroy(struct net_buf *buf)
{
	net_buf_destroy(buf);

	if (buf_rx_freed_cb) {
		/* bt_buf_get_rx is used for all types of RX buffers */
		buf_rx_freed_cb(BT_BUF_EVT | BT_BUF_ACL_IN | BT_BUF_ISO_IN);
	}
}

NET_BUF_POOL_FIXED_DEFINE(hci_rx_pool, BT_BUF_RX_COUNT, BT_BUF_RX_SIZE, 0, hci_rx_buf_destroy);
NET_BUF_POOL_FIXED_DEFINE(hci_cmd_pool, BT_BUF_CMD_TX_COUNT,
			  BT_BUF_CMD_SIZE(CONFIG_BT_BUF_CMD_TX_SIZE), 0, NULL);
NET_BUF_POOL_FIXED_DEFINE(hci_acl_pool, CONFIG_BT_BUF_ACL_TX_COUNT,
			  BT_BUF_ACL_SIZE(CONFIG_BT_BUF_ACL_TX_SIZE), 0, NULL);
#if defined(CONFIG_BT_ISO)
NET_BUF_POOL_FIXED_DEFINE(hci_iso_pool, CONFIG_BT_ISO_TX_BUF_COUNT,
			  BT_ISO_SDU_BUF_SIZE(CONFIG_BT_ISO_TX_MTU), 0, NULL);
#endif /* CONFIG_BT_ISO */

struct bt_dev_raw bt_dev;

struct net_buf *bt_buf_get_rx(enum bt_buf_type type, bt_timeout_t timeout)
{
	struct net_buf *buf;

	switch (type) {
	case BT_BUF_EVT:
	case BT_BUF_ACL_IN:
	case BT_BUF_ISO_IN:
		break;
	default:
		LOG_ERR("Invalid rx type: %u", type);
		return NULL;
	}

	buf = net_buf_alloc(&hci_rx_pool, timeout);
	if (!buf) {
		return buf;
	}

	net_buf_add_u8(buf, bt_buf_type_to_h4(type));

	return buf;
}

void bt_buf_rx_freed_cb_set(bt_buf_rx_freed_cb_t cb)
{
	buf_rx_freed_cb = cb;
}

struct net_buf *bt_buf_get_tx(enum bt_buf_type type, bt_timeout_t timeout,
			      const void *data, size_t size)
{
	struct net_buf_pool *pool;
	struct net_buf *buf;

	switch (type) {
	case BT_BUF_CMD:
		pool = &hci_cmd_pool;
		break;
	case BT_BUF_ACL_OUT:
		pool = &hci_acl_pool;
		break;
#if defined(CONFIG_BT_ISO)
	case BT_BUF_ISO_OUT:
		pool = &hci_iso_pool;
		break;
#endif /* CONFIG_BT_ISO */
	default:
		LOG_ERR("Invalid tx type: %u", type);
		return NULL;
	}

	buf = net_buf_alloc(pool, timeout);
	if (!buf) {
		return buf;
	}

	net_buf_add_u8(buf, bt_buf_type_to_h4(type));

	if (data && size) {
		if (net_buf_tailroom(buf) < size) {
			net_buf_unref(buf);
			return NULL;
		}

		net_buf_add_mem(buf, data, size);
	}

	return buf;
}

struct net_buf *bt_buf_get_evt(uint8_t evt, bool discardable, bt_timeout_t timeout)
{
	ARG_UNUSED(evt);
	ARG_UNUSED(discardable);

	return bt_buf_get_rx(BT_BUF_EVT, timeout);
}

static int bt_recv(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	uint8_t type = buf->data[0];

	ARG_UNUSED(dev);

	LOG_DBG("buf %p len %u", buf, buf->len);

	bt_monitor_send(bt_monitor_opcode(type, BT_MONITOR_RX), buf->data + 1, buf->len - 1);

	bt_fifo_put(raw_rx, net_buf_take(&buf));

	return 0;
}

int bt_send(struct net_buf *buf)
{
	if (buf->len == 0) {
		return BT_HCI_ERR_INVALID_PARAM;
	}

	LOG_DBG("buf %p type %u len %u", buf, buf->data[0], buf->len);

	bt_monitor_send(bt_monitor_opcode(buf->data[0], BT_MONITOR_TX),
			buf->data + 1, buf->len - 1);

	return bt_hci_send(bt_dev.hci, buf);
}

void bt_hci_raw_init(void)
{
	static bool initialized;

	if (initialized) {
		return;
	}
	net_buf_pool_init(&hci_rx_pool);
	net_buf_pool_init(&hci_cmd_pool);
	net_buf_pool_init(&hci_acl_pool);
#if defined(CONFIG_BT_ISO)
	net_buf_pool_init(&hci_iso_pool);
#endif
	initialized = true;
}

int bt_enable_raw(struct bt_fifo *rx_queue)
{
	bt_hci_raw_init();
	bt_fifo_init(rx_queue);

	int err;

	LOG_DBG("");

	raw_rx = rx_queue;

	if (!bt_dev.hci || !bt_dev.hci->api) {
		LOG_ERR("HCI driver is not ready");
		return -ENODEV;
	}

	bt_monitor_new_index(BT_MONITOR_TYPE_PRIMARY, bt_dev.hci->bus, BT_ADDR_ANY, bt_dev.hci->name);

	err = bt_hci_open(bt_dev.hci, bt_recv);
	if (err) {
		LOG_ERR("HCI driver open failed (%d)", err);
		return err;
	}

	LOG_INF("Lower HCI transport: %s", bt_dev.hci->name);
	LOG_INF("Bluetooth enabled in RAW mode");

	return 0;
}

int bt_hci_raw_transport_set(const struct bt_hci_transport *transport)
{
	if (!transport || !transport->api) {
		return -EINVAL;
	}
	if (raw_rx) {
		return -EBUSY;
	}
	bt_dev.hci = transport;
	return 0;
}
