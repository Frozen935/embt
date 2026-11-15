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
 * @brief H4 framing and Host buffer ownership for all supported socket transports.
 */

#include <base/base.h>
#include <embt/drivers/bluetooth.h>
#include <embt/bluetooth/transport.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/sys/byteorder.h>
#include <host/hci_core.h>
#include "socket_io.h"
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

struct socket_context {
	struct bt_hci_driver_data common;
	struct bt_hci_socket_config config;
	char address[256];
	int fd;
	int stop_fd;
	os_thread_t receiver;
	os_mutex_t send_lock;
	bt_atomic_t stopping;
	bt_atomic_t failure;
};
static struct socket_context context = {.fd = -1, .stop_fd = -1};
static struct bt_hci_transport transport;

/** Return zero for an incomplete header, or the full H4 packet length. */
static int packet_size(const uint8_t *data, size_t size)
{
	if (size == 0) {
		return 0;
	}
	switch (data[0]) {
	case BT_HCI_H4_EVT:
		return size < 3 ? 0 : 3 + data[2];
	case BT_HCI_H4_ACL:
		return size < 5 ? 0 : 5 + sys_get_le16(data + 3);
	case BT_HCI_H4_SCO:
		return size < 4 ? 0 : 4 + data[3];
	default:
		return -EPROTO;
	}
}

static int deliver(const struct bt_hci_transport *dev, const uint8_t *data, size_t size)
{
	struct socket_context *ctx = dev->data;
	struct net_buf *buf = NULL;

	/* Preserve backpressure while keeping close bounded during pool exhaustion. */
	while (!bt_atomic_get(&ctx->stopping)) {
		if (data[0] == BT_HCI_H4_EVT) {
			buf = bt_buf_get_evt(data[1], false, K_MSEC(50));
		} else if (data[0] == BT_HCI_H4_ACL) {
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_MSEC(50));
		} else {
			/* The retained Host handles SCO connection control, not HCI SCO payloads.
			 */
			return -ENOTSUP;
		}
		if (buf) {
			break;
		}
	}
	if (!buf) {
		return -ECANCELED;
	}
	if (net_buf_tailroom(buf) < size - 1) {
		net_buf_unref(buf);
		return -EMSGSIZE;
	}
	net_buf_add_mem(buf, data + 1, size - 1);
	bt_hci_recv(dev, buf);
	return 0;
}

static void receive_thread(void *arg)
{
	const struct bt_hci_transport *dev = arg;
	struct socket_context *ctx = dev->data;
	uint8_t data[BT_BUF_RX_SIZE + 5];
	size_t used = 0;
	int err = 0;
	bool packet = ctx->config.kind == BT_HCI_SOCKET_USER;

	while (!bt_atomic_get(&ctx->stopping)) {
		int count = bt_socket_read(ctx->fd, ctx->stop_fd, data + used, sizeof(data) - used,
					   packet);
		size_t offset = 0;

		if (count < 0) {
			err = count;
			break;
		}
		used += count;
		while (offset < used) {
			int length = packet_size(data + offset, used - offset);

			if (length < 0 || length > (int)sizeof(data)) {
				err = length < 0 ? length : -EMSGSIZE;
				goto done;
			}
			if (length == 0 || (size_t)length > used - offset) {
				if (packet) {
					err = -EPROTO;
					goto done;
				}
				break;
			}
			if (packet && (size_t)length != used) {
				err = -EPROTO;
				goto done;
			}
			err = deliver(dev, data + offset, length);
			if (err && err != -ENOTSUP) {
				goto done;
			}
			offset += length;
		}
		used -= offset;
		memmove(data, data + offset, used);
	}
done:
	if (err != -ECANCELED) {
		bt_atomic_set(&ctx->failure, err ? err : -ECONNRESET);
	}
}

static int socket_open(const struct bt_hci_transport *dev)
{
	struct socket_context *ctx = dev->data;
	int err;

	if (ctx->fd >= 0) {
		return -EALREADY;
	}
	ctx->fd = bt_socket_connect(&ctx->config);
	if (ctx->fd < 0) {
		err = ctx->fd;
		ctx->fd = -1;
		return err;
	}
	ctx->stop_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (ctx->stop_fd < 0) {
		err = -errno;
		goto fail_socket;
	}
	err = osi_mutex_init(&ctx->send_lock, OS_MUTEX_NORMAL);
	if (err) {
		goto fail_event;
	}
	bt_atomic_clear(&ctx->stopping);
	bt_atomic_clear(&ctx->failure);
	err = osi_thread_create(&ctx->receiver, receive_thread, (void *)dev, "embt-hci-rx", 0, 0);
	if (err) {
		osi_mutex_deinit(&ctx->send_lock);
		goto fail_event;
	}
	return 0;
fail_event:
	close(ctx->stop_fd);
	ctx->stop_fd = -1;
fail_socket:
	close(ctx->fd);
	ctx->fd = -1;
	return err;
}

static int socket_close(const struct bt_hci_transport *dev)
{
	struct socket_context *ctx = dev->data;
	uint64_t wake = 1;
	int err;

	if (ctx->fd < 0) {
		return -ENETDOWN;
	}
	if (osi_thread_is_current(&ctx->receiver)) {
		return -EDEADLK;
	}
	bt_atomic_set(&ctx->stopping, 1);
	if (write(ctx->stop_fd, &wake, sizeof(wake)) < 0 && errno != EAGAIN) {
		bt_atomic_clear(&ctx->stopping);
		return -errno;
	}
	err = osi_thread_join(&ctx->receiver);
	if (err) {
		return err;
	}
	close(ctx->fd);
	close(ctx->stop_fd);
	ctx->fd = ctx->stop_fd = -1;
	return osi_mutex_deinit(&ctx->send_lock);
}

static int socket_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	struct socket_context *ctx = dev->data;
	int err;

	if (ctx->fd < 0) {
		return -ENETDOWN;
	}
	bt_base_require(osi_mutex_lock(&ctx->send_lock, OS_TIMEOUT_FOREVER));
	err = (int)bt_atomic_get(&ctx->failure);
	if (!err) {
		err = bt_socket_write(ctx->fd, ctx->stop_fd, buf->data, buf->len,
				      ctx->config.kind == BT_HCI_SOCKET_USER);
	}
	if (err) {
		/* A partial stream packet cannot safely be retried on this connection. */
		bt_atomic_set(&ctx->failure, err);
		shutdown(ctx->fd, SHUT_RDWR);
	}
	bt_base_require(osi_mutex_unlock(&ctx->send_lock));
	if (!err) {
		net_buf_unref(buf);
	}
	return err;
}

static const struct bt_hci_driver_api socket_api = {
	.open = socket_open,
	.close = socket_close,
	.send = socket_send,
};

int bt_hci_socket_configure(const struct bt_hci_socket_config *config)
{
	if (!config || config->kind > BT_HCI_SOCKET_UNIX || config->kind < BT_HCI_SOCKET_USER) {
		return -EINVAL;
	}
	if (context.fd >= 0 || bt_atomic_test_bit(bt_dev.flags, BT_DEV_ENABLING)) {
		return -EBUSY;
	}
	if (config->kind != BT_HCI_SOCKET_USER) {
		if (!config->address || !config->address[0] ||
		    strlen(config->address) >= sizeof(context.address)) {
			return -EINVAL;
		}
		if (config->kind == BT_HCI_SOCKET_TCP && config->port == 0) {
			return -EINVAL;
		}
		if (config->kind == BT_HCI_SOCKET_UNIX && config->address[0] != '/') {
			return -EINVAL;
		}
		strcpy(context.address, config->address);
	}
	context.config = *config;
	context.config.address = context.address;
	transport = (struct bt_hci_transport){
		.name = "EmBT socket",
		.bus = 0,
		.quirks = config->quirks,
		.api = &socket_api,
		.data = &context,
	};
	bt_dev.hci = &transport;
	return 0;
}
