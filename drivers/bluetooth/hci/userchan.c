/* userchan.c - HCI User Channel based Bluetooth driver */

/*
 * Copyright (c) 2018 Intel Corporation
 * Copyright (c) 2023 Victor Chavez
 * SPDX-License-Identifier: Apache-2.0
 */



#include <embt/base/time.h>
#include <embt/osi/thread.h>
#include <embt/osi/time.h>
#include <embt/base/assert.h>
#include <embt/base/util.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#if defined(CONFIG_BT_HCI_RAW)
#include <embt/bluetooth/hci_raw.h>
#include "host/hci_raw_internal.h"
#else
#include "host/hci_core.h"
#endif
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <embt/base/byteorder.h>

#include "userchan_bottom.h"

#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/hci.h>
#include <embt/drivers/bluetooth.h>
#include <embt/base/log.h>

LOG_MODULE_REGISTER(bt_driver, CONFIG_BT_HCI_DRIVER_LOG_LEVEL);


struct uc_data {
	/* bt_hci_driver_data must be first */
	struct bt_hci_driver_data common;
	int           fd;
};

static os_thread_t rx_thread_data;

static unsigned short bt_dev_index;

#define TCP_ADDR_BUFF_SIZE 16
#define UNIX_ADDR_BUFF_SIZE 4096

/* Fit the largest packet the host can receive. BT_BUF_RX_SIZE covers the H4
 * type octet and the maximum of the ACL, event and ISO receive-buffer sizes,
 * so it holds every packet type get_rx() accepts, including ISO.
 */
#define RX_FRAME_SIZE BT_BUF_RX_SIZE
enum hci_connection_type {
	HCI_USERCHAN,
	HCI_TCP,
	HCI_UNIX,
};
static enum hci_connection_type conn_type;
static char ip_addr[TCP_ADDR_BUFF_SIZE];
static unsigned int port;
static char socket_path[UNIX_ADDR_BUFF_SIZE];

static bool is_hci_event_discardable(const struct bt_hci_evt_hdr *evt)
{
	switch (evt->evt) {
#if defined(CONFIG_BT_CLASSIC)
	case BT_HCI_EVT_INQUIRY_RESULT_WITH_RSSI:
	case BT_HCI_EVT_EXTENDED_INQUIRY_RESULT:
		return true;
#endif
	case BT_HCI_EVT_LE_META_EVENT: {
		const struct bt_hci_evt_le_meta_event *meta_evt = (const void *)evt->data;

		switch (meta_evt->subevent) {
		case BT_HCI_EVT_LE_ADVERTISING_REPORT:
			return true;
#if defined(CONFIG_BT_EXT_ADV)
		case BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT: {
			const struct bt_hci_evt_le_ext_advertising_report *ext_adv =
				(const void *)meta_evt->data;
			uint16_t adv_evt_type = sys_le16_to_cpu(ext_adv->adv_info[0].evt_type);

			return (ext_adv->num_reports == 1) &&
			       ((adv_evt_type & BT_HCI_LE_ADV_EVT_TYPE_LEGACY) != 0);
		}
#endif
		default:
			return false;
		}
	}
	default:
		return false;
	}
}

static struct net_buf *get_rx_evt(const uint8_t *data)
{
	const struct bt_hci_evt_hdr *evt = (const void *)data;
	const bool discardable = is_hci_event_discardable(evt);
	const bt_timeout_t timeout = discardable ? BT_NO_WAIT : BT_SECONDS(1);
	struct net_buf *buf;

	do {
		buf = bt_buf_get_evt(evt->evt, discardable, timeout);
		if (buf == NULL) {
			if (discardable) {
				LOG_DBG_RATELIMIT("Discardable buffer pool full, ignoring event");
				return buf;
			}
			LOG_WRN("Couldn't allocate a buffer after waiting 1 second.");
		}
	} while (!buf);

	return buf;
}

static struct net_buf *get_rx_acl(const uint8_t *data)
{
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_NO_WAIT);
	if (buf == NULL) {
		LOG_ERR("No available ACL buffers!");
	}

	return buf;
}

static struct net_buf *get_rx_iso(const uint8_t *data)
{
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_ISO_IN, BT_NO_WAIT);
	if (buf == NULL) {
		LOG_ERR_RATELIMIT("No available ISO buffers!");
	}

	return buf;
}

static struct net_buf *get_rx(const uint8_t *buf)
{
	uint8_t hci_h4_type = buf[0];

	switch (hci_h4_type) {
	case BT_HCI_H4_EVT:
		return get_rx_evt(&buf[1]);
	case BT_HCI_H4_ACL:
		return get_rx_acl(&buf[1]);
	case BT_HCI_H4_ISO:
		if (IS_ENABLED(CONFIG_BT_ISO)) {
			return get_rx_iso(&buf[1]);
		}
		__fallthrough;
	default:
		LOG_ERR("Unknown packet type: %u", hci_h4_type);
	}

	return NULL;
}

/**
 * @brief Decode the length of an HCI H4 packet and check it's complete
 * @details Decodes packet length according to Bluetooth spec v5.4 Vol 4 Part E
 * @param buf	Pointer to a HCI packet buffer
 * @param buf_len	Bytes available in the buffer
 * @return Length of the complete HCI packet in bytes, -1 if cannot find an HCI
 *         packet, 0 if more data required.
 */
static int32_t hci_packet_complete(const uint8_t *buf, uint16_t buf_len)
{
	uint16_t payload_len = 0;
	const uint8_t type = buf[0];
	uint8_t header_len = sizeof(type);
	const uint8_t *hdr = &buf[sizeof(type)];

	switch (type) {
	case BT_HCI_H4_CMD: {
		if (buf_len < header_len + BT_HCI_CMD_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_cmd_hdr *cmd = (const struct bt_hci_cmd_hdr *)hdr;

		/* Parameter Total Length */
		payload_len = cmd->param_len;
		header_len += BT_HCI_CMD_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_ACL: {
		if (buf_len < header_len + BT_HCI_ACL_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_acl_hdr *acl = (const struct bt_hci_acl_hdr *)hdr;

		/* Data Total Length */
		payload_len = sys_le16_to_cpu(acl->len);
		header_len += BT_HCI_ACL_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_SCO: {
		if (buf_len < header_len + BT_HCI_SCO_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_sco_hdr *sco = (const struct bt_hci_sco_hdr *)hdr;

		/* Data_Total_Length */
		payload_len = sco->len;
		header_len += BT_HCI_SCO_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_EVT: {
		if (buf_len < header_len + BT_HCI_EVT_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_evt_hdr *evt = (const struct bt_hci_evt_hdr *)hdr;

		/* Parameter Total Length */
		payload_len = evt->len;
		header_len += BT_HCI_EVT_HDR_SIZE;
		break;
	}
	case BT_HCI_H4_ISO: {
		if (buf_len < header_len + BT_HCI_ISO_HDR_SIZE) {
			return 0;
		}
		const struct bt_hci_iso_hdr *iso = (const struct bt_hci_iso_hdr *)hdr;

		/* ISO_Data_Load_Length parameter */
		payload_len =  bt_iso_hdr_len(sys_le16_to_cpu(iso->len));
		header_len += BT_HCI_ISO_HDR_SIZE;
		break;
	}
	/* If no valid packet type found */
	default:
		LOG_WRN("Unknown packet type 0x%02x", type);
		return -1;
	}

	/* Request more data */
	if (buf_len < header_len + payload_len) {
		return 0;
	}

	return (int32_t)header_len + payload_len;
}

static void rx_thread(void *p1)
{
	const struct bt_hci_transport *dev = p1;
	struct uc_data *uc = dev->data;


	LOG_DBG("started");

	long frame_size = 0;

	while (1) {
		static uint8_t frame[RX_FRAME_SIZE];
		struct net_buf *buf;
		size_t buf_tailroom;
		size_t buf_add_len;
		long len;
		const uint8_t *frame_start = frame;

		if (!user_chan_rx_ready(uc->fd)) {
			osi_sleep_ms(BT_MSEC(1));
			continue;
		}

		if (frame_size >= sizeof(frame)) {
			LOG_ERR("HCI Packet is too big for frame (%d "
				"bytes). Dropping data", sizeof(frame));
			frame_size = 0; /* Drop buffer */
		}

		LOG_DBG("calling read()");

		len = read(uc->fd, frame + frame_size, sizeof(frame) - frame_size);
		if (len < 0) {
			if (errno == EINTR) {
				osi_thread_yield();
				continue;
			}

			LOG_ERR("Reading socket failed, errno %d", errno);
			(void)close(uc->fd);
			uc->fd = -1;
			return;
		}

		frame_size += len;

		while (frame_size > 0) {
			const uint8_t *buf_add;
			const uint8_t packet_type = frame_start[0];
			const int32_t decoded_len = hci_packet_complete(frame_start, frame_size);

			if (decoded_len == -1) {
				LOG_ERR("HCI Packet type is invalid, length could not be decoded");
				frame_size = 0; /* Drop buffer */
				break;
			}

			if (decoded_len == 0) {
				if ((frame_start != frame) && (frame_size < sizeof(frame))) {
					memmove(frame, frame_start, frame_size);
				}
				/* Read more */
				break;
			}

			buf_add = frame_start + sizeof(packet_type);
			buf_add_len = decoded_len - sizeof(packet_type);

			buf = get_rx(frame_start);

			frame_size -= decoded_len;
			frame_start += decoded_len;

			if (!buf) {
				continue;
			}

			buf_tailroom = net_buf_tailroom(buf);
			if (buf_tailroom < buf_add_len) {
				LOG_ERR("Not enough space in buffer %zu/%zu",
					buf_add_len, buf_tailroom);
				net_buf_unref(buf);
				continue;
			}

			net_buf_add_mem(buf, buf_add, buf_add_len);

			LOG_DBG("Calling bt_hci_recv(%p)", buf);

			bt_hci_recv(dev, buf);
		}

		osi_thread_yield();
	}
}

static int uc_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	struct uc_data *uc = dev->data;

	LOG_DBG("buf %p type %u len %u", buf, buf->data[0], buf->len);

	if (uc->fd < 0) {
		LOG_ERR("User channel not open");
		return -EIO;
	}

	if (write(uc->fd, buf->data, buf->len) < 0) {
		return -errno;
	}

	net_buf_unref(buf);
	return 0;
}

static int uc_open(const struct bt_hci_transport *dev)
{
	struct uc_data *uc = dev->data;

	switch (conn_type) {
	case HCI_USERCHAN:
		LOG_DBG("hci%d", bt_dev_index);
		uc->fd = user_chan_socket_open(bt_dev_index);
		break;
	case HCI_TCP:
		LOG_DBG("hci %s:%d", ip_addr, port);
		uc->fd = user_chan_net_connect(ip_addr, port);
		break;
	case HCI_UNIX:
		LOG_DBG("hci socket %s", socket_path);
		uc->fd = user_chan_unix_connect(socket_path);
		break;
	}
	if (uc->fd < 0) {
		return uc->fd;
	}

	LOG_DBG("User Channel opened as fd %d", uc->fd);

	int err = osi_thread_create(&rx_thread_data, rx_thread, (void *)dev,
				    "BT RX", CONFIG_BT_DRIVER_RX_HIGH_PRIO, CONFIG_BT_RX_STACK_SIZE);
	if (err) {
		close(uc->fd);
		uc->fd = -1;
		return err;
	}

	LOG_DBG("returning");

	return 0;
}

static int uc_close(const struct bt_hci_transport *dev)
{
	struct uc_data *uc = dev->data;
	int rc;

	if (uc->fd < 0) {
		return -ENETDOWN;
	}

	rc = close(uc->fd);
	if (rc < 0) {
		return -errno;
	}

	uc->fd = -1;
	osi_thread_cancel(&rx_thread_data);
	return osi_thread_join(&rx_thread_data);
}

static const struct bt_hci_driver_api uc_drv_api = {
	.open = uc_open,
	.close = uc_close,
	.send = uc_send,
};


static struct uc_data uc_context = {.fd = -1};
static struct bt_hci_transport uc_transport = {
	.name = "Linux HCI user channel",
	.bus = 1,
	.api = &uc_drv_api,
	.data = &uc_context,
};

/** @brief Select the retained Linux driver explicitly, without simulator startup. */
int bt_hci_userchan_configure(const char *endpoint)
{
	char *end;
	unsigned long number;

	if (!endpoint || !*endpoint || uc_context.fd >= 0) {
		return -EINVAL;
	}
	if (!strncmp(endpoint, "hci", 3)) {
		errno = 0;
		number = strtoul(endpoint + 3, &end, 10);
		if (errno || end == endpoint + 3 || *end || number > USHRT_MAX) {
			LOG_ERR("Invalid HCI device index");
			return -EINVAL;
		}
		bt_dev_index = number;
		conn_type = HCI_USERCHAN;
	} else if (*endpoint == '/') {
		if (strlen(endpoint) >= sizeof(socket_path)) {
			return -ENAMETOOLONG;
		}
		strcpy(socket_path, endpoint);
		conn_type = HCI_UNIX;
	} else {
		int used = 0;

		if (sscanf(endpoint, "%15[^:]:%u%n", ip_addr, &port, &used) != 2 ||
		    endpoint[used] || !port || port > USHRT_MAX ||
		    user_chan_is_ipaddr_ok(ip_addr) != 1) {
			LOG_ERR("Invalid IPv4 HCI endpoint");
			return -EINVAL;
		}
		conn_type = HCI_TCP;
	}
#if defined(CONFIG_BT_HCI_RAW)
	return bt_hci_raw_transport_set(&uc_transport);
#else
	bt_dev.hci = &uc_transport;
	return 0;
#endif
}
