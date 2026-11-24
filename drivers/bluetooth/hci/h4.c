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
 * @brief H4 device transport using the OpenVela zblue port read/write contract.
 */

#include <errno.h>
#include <embt/base/time.h>
#include <embt/base/base.h>
#include <embt/base/atomic.h>
#include <embt/osi/sync.h>
#include <embt/osi/thread.h>
#include <embt/osi/time.h>
#include <string.h>
#include <embt/base/assert.h>
#include <embt/drivers/bluetooth.h>
#include <embt/bluetooth/transport.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/base/byteorder.h>
#if defined(CONFIG_BT_HCI_RAW)
#include <embt/bluetooth/hci_raw.h>
#include "host/hci_raw_internal.h"
#else
#include "host/hci_core.h"
#endif
#include <fcntl.h>
#include <poll.h>
#include <embt/base/log.h>
#include <sys/eventfd.h>
#include <unistd.h>

struct h4_context {
	struct bt_hci_driver_data common;
	char path[128];
	int fd;
	int stop_fd;
	os_thread_t receiver;
	os_mutex_t send_lock;
	bt_atomic_t stopping;
	bt_atomic_t failure;
};
static struct h4_context context = {.fd = -1, .stop_fd = -1};
static struct bt_hci_transport transport;

LOG_MODULE_REGISTER(bt_h4, CONFIG_BT_HCI_DRIVER_LOG_LEVEL);

static int h4_wait(int fd, int stop_fd, short events, int timeout)
{
	struct pollfd fds[2] = {{.fd = fd, .events = events},
			      {.fd = stop_fd, .events = POLLIN}};
	uint64_t start = 0, now;
	int rc;

	if (timeout >= 0) {
		rc = osi_time_get_ms(&start);
		if (rc) {
			return rc;
		}
	}
	for (;;) {
		int remaining = -1;

		if (timeout >= 0) {
			rc = osi_time_get_ms(&now);
			if (rc || now - start >= (uint64_t)timeout) {
				return rc ? rc : -ETIMEDOUT;
			}
			remaining = timeout - (int)(now - start);
		}
		rc = poll(fds, 2, remaining);
		if (rc >= 0 || errno != EINTR) {
			break;
		}
	}
	if (rc < 0) {
		return -errno;
	}
	if (rc == 0) {
		return -ETIMEDOUT;
	}
	if (fds[1].revents) {
		return -ECANCELED;
	}
	if (fds[0].revents & events) {
		return 0;
	}
	return -EIO;
}

static int h4_read(int fd, int stop_fd, void *data, size_t size)
{
	for (;;) {
		int err = h4_wait(fd, stop_fd, POLLIN, -1);
		ssize_t count;

		if (err) {
			return err;
		}
		count = read(fd, data, size);
		if (count < 0 && (errno == EINTR || errno == EAGAIN)) {
			continue;
		}
		if (count < 0) {
			return -errno;
		}
		return count == 0 ? -EIO : (int)count;
	}
}

static int h4_write(int fd, int stop_fd, const void *data, size_t size)
{
	size_t offset = 0;
	uint64_t start, now;
	int err = osi_time_get_ms(&start);

	if (err) {
		return err;
	}
	while (offset < size) {
		ssize_t count = write(fd, (const uint8_t *)data + offset, size - offset);

		if (count > 0) {
			offset += count;
			continue;
		}
		if (count < 0 && errno == EINTR) {
			continue;
		}
		if (count == 0 || errno != EAGAIN) {
			return count == 0 ? -EIO : -errno;
		}
		err = osi_time_get_ms(&now);
		if (err || now - start >= 5000) {
			return err ? err : -ETIMEDOUT;
		}
		err = h4_wait(fd, stop_fd, POLLOUT, (int)(5000 - (now - start)));
		if (err) {
			return err;
		}
	}
	return 0;
}

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
	struct h4_context *ctx = dev->data;
	struct net_buf *buf = NULL;

	/* Preserve backpressure while keeping close bounded during pool exhaustion. */
	while (!bt_atomic_get(&ctx->stopping)) {
		if (data[0] == BT_HCI_H4_EVT) {
			buf = bt_buf_get_evt(data[1], false, BT_MSEC(50));
		} else if (data[0] == BT_HCI_H4_ACL) {
			buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_MSEC(50));
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
	struct h4_context *ctx = dev->data;
	uint8_t data[BT_BUF_RX_SIZE + 5];
	size_t used = 0;
	int err = 0;


	while (!bt_atomic_get(&ctx->stopping)) {
		int count = h4_read(ctx->fd, ctx->stop_fd, data + used, sizeof(data) - used);
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
				break;
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
		LOG_ERR("H4 receive failed (%d)", err);
		bt_atomic_set(&ctx->failure, err ? err : -ECONNRESET);
	}
}

static int h4_open(const struct bt_hci_transport *dev)
{
	struct h4_context *ctx = dev->data;
	int flags = O_RDWR | O_NONBLOCK | O_CLOEXEC;
	int err;

	if (ctx->fd >= 0) {
		return -EALREADY;
	}

#if defined(__NuttX__)
	flags |= O_BINARY;
#endif
	ctx->fd = open(ctx->path, flags);
	LOG_DBG("Opening H4 device %s", ctx->path);
	if (ctx->fd < 0) {
		err = -errno;
		ctx->fd = -1;
		return err;
	}
	ctx->stop_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if (ctx->stop_fd < 0) {
		err = -errno;
		goto fail_device;
	}
	err = osi_mutex_init(&ctx->send_lock, OS_MUTEX_NORMAL);
	if (err) {
		goto fail_event;
	}
	bt_atomic_clear(&ctx->stopping);
	bt_atomic_clear(&ctx->failure);
	err = osi_thread_create(&ctx->receiver, receive_thread, (void *)dev, "embt-h4-rx",
				CONFIG_BT_DRIVER_RX_HIGH_PRIO, CONFIG_BT_RX_STACK_SIZE);
	if (err) {
		osi_mutex_deinit(&ctx->send_lock);
		goto fail_event;
	}
	return 0;
fail_event:
	close(ctx->stop_fd);
	ctx->stop_fd = -1;
fail_device:
	close(ctx->fd);
	ctx->fd = -1;
	return err;
}

static int h4_close(const struct bt_hci_transport *dev)
{
	struct h4_context *ctx = dev->data;
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

static int h4_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	struct h4_context *ctx = dev->data;
	int err;

	if (ctx->fd < 0) {
		return -ENETDOWN;
	}
	bt_base_require(osi_mutex_lock(&ctx->send_lock, OS_TIMEOUT_FOREVER));
	err = (int)bt_atomic_get(&ctx->failure);
	if (!err) {
		err = h4_write(ctx->fd, ctx->stop_fd, buf->data, buf->len);
	}
	if (err) {
		/* A partial stream packet cannot safely be retried on this connection. */
		bt_atomic_set(&ctx->failure, err);
	}
	bt_base_require(osi_mutex_unlock(&ctx->send_lock));
	if (!err) {
		net_buf_unref(buf);
	}
	return err;
}

static const struct bt_hci_driver_api h4_api = {
	.open = h4_open,
	.close = h4_close,
	.send = h4_send,
};

int bt_hci_h4_configure(const char *path, uint32_t quirks)
{
	if (!path || path[0] != '/' || strlen(path) >= sizeof(context.path)) {
		return -EINVAL;
	}
	if (context.fd >= 0
#if !defined(CONFIG_BT_HCI_RAW)
	    || bt_atomic_test_bit(bt_dev.flags, BT_DEV_ENABLING)
#endif
	) {
		return -EBUSY;
	}
	strcpy(context.path, path);
	transport = (struct bt_hci_transport){
		.name = "EmBT H4 device",
		.bus = 0,
		.quirks = quirks,
		.api = &h4_api,
		.data = &context,
	};
#if defined(CONFIG_BT_HCI_RAW)
	return bt_hci_raw_transport_set(&transport);
#else
	bt_dev.hci = &transport;
	return 0;
#endif
}
