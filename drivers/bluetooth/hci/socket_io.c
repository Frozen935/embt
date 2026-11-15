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
 * @brief Interruptible stream and Linux HCI user socket operations.
 */

#define _GNU_SOURCE
#include "socket_io.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
/* Linux HCI socket ABI: family, controller index, channel (USER = 1).
 * These socket operations do not require the BlueZ userspace library.
 */
struct bt_linux_hci_address {
	sa_family_t family;
	unsigned short controller;
	unsigned short channel;
};

/* Linux HCI ioctl ABI, matching HCIDEVDOWN in the BlueZ HCI headers. */
#define BT_LINUX_HCIDEVDOWN _IOW('H', 202, int)

static int64_t monotonic_ms(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
		return -1;
	}
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int ready(int fd, int stop, short events, int64_t deadline)
{
	struct pollfd fds[2] = {{.fd = fd, .events = events}, {.fd = stop, .events = POLLIN}};

	for (;;) {
		int timeout = -1;
		int ret;

		if (deadline >= 0) {
			int64_t remaining = deadline - monotonic_ms();

			if (remaining <= 0) {
				return -ETIMEDOUT;
			}
			timeout = remaining > INT32_MAX ? INT32_MAX : (int)remaining;
		}
		ret = poll(fds, 2, timeout);
		if (ret < 0 && errno == EINTR) {
			continue;
		}
		if (ret <= 0) {
			return ret == 0 ? -ETIMEDOUT : -errno;
		}
		if (fds[1].revents) {
			return -ECANCELED;
		}
		if (fds[0].revents & events) {
			return 0;
		}
		if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			return -ECONNRESET;
		}
	}
}

static int connect_one(int domain, int type, int protocol, const struct sockaddr *addr,
		       socklen_t len)
{
	int fd = socket(domain, type | SOCK_CLOEXEC | SOCK_NONBLOCK, protocol);
	int err;

	if (fd < 0) {
		return -errno;
	}
	if (connect(fd, addr, len) == 0) {
		return fd;
	}
	err = -errno;
	if (errno == EINPROGRESS) {
		err = ready(fd, -1, POLLOUT, monotonic_ms() + 5000);
		if (!err) {
			socklen_t size = sizeof(err);

			if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &size) < 0) {
				err = errno;
			}
			err = -err;
		}
	}
	if (err) {
		close(fd);
		return err;
	}
	return fd;
}

int bt_socket_connect(const struct bt_hci_socket_config *config)
{
	if (config->kind == BT_HCI_SOCKET_USER) {
		struct bt_linux_hci_address addr = {
			.family = AF_BLUETOOTH,
			.controller = config->controller,
			.channel = 1,
		};
		int fd = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, 1);

		if (fd < 0) {
			return -errno;
		}
		/* Release this controller from the kernel Host before taking user-channel
		 * ownership. */
		if (ioctl(fd, BT_LINUX_HCIDEVDOWN, (unsigned long)config->controller) < 0 &&
		    errno != EALREADY) {
			int err = -errno;

			close(fd);
			return err;
		}
		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			int err = -errno;

			close(fd);
			return err;
		}
		return fd;
	}
	if (config->kind == BT_HCI_SOCKET_UNIX) {
		struct sockaddr_un addr = {.sun_family = AF_UNIX};

		if (strlen(config->address) >= sizeof(addr.sun_path)) {
			return -ENAMETOOLONG;
		}
		strcpy(addr.sun_path, config->address);
		return connect_one(AF_UNIX, SOCK_STREAM, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
	if (config->kind == BT_HCI_SOCKET_TCP) {
		struct addrinfo hints = {.ai_socktype = SOCK_STREAM, .ai_family = AF_UNSPEC};
		struct addrinfo *list;
		char port[6];
		int err = -ECONNREFUSED;

		snprintf(port, sizeof(port), "%u", config->port);
		if (getaddrinfo(config->address, port, &hints, &list) != 0) {
			return -EHOSTUNREACH;
		}
		for (struct addrinfo *p = list; p; p = p->ai_next) {
			err = connect_one(p->ai_family, p->ai_socktype, p->ai_protocol, p->ai_addr,
					  p->ai_addrlen);
			if (err >= 0) {
				break;
			}
		}
		freeaddrinfo(list);
		return err;
	}
	return -EINVAL;
}

int bt_socket_read(int fd, int stop_fd, void *data, size_t size, int packet)
{
	for (;;) {
		int err = ready(fd, stop_fd, POLLIN, -1);
		ssize_t count;

		if (err) {
			return err;
		}
		count = recv(fd, data, size, packet ? MSG_TRUNC : 0);
		if (count < 0 && (errno == EAGAIN || errno == EINTR)) {
			continue;
		}
		if (count < 0) {
			return -errno;
		}
		if ((size_t)count > size) {
			return -EMSGSIZE;
		}
		return count == 0 ? -ECONNRESET : (int)count;
	}
}

int bt_socket_write(int fd, int stop_fd, const void *data, size_t size, int packet)
{
	size_t offset = 0;
	int64_t deadline = monotonic_ms() + 5000;

	while (offset < size) {
		ssize_t count = send(fd, (const char *)data + offset, size - offset, MSG_NOSIGNAL);

		if (count < 0 && errno == EINTR) {
			continue;
		}
		if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			int err = ready(fd, stop_fd, POLLOUT, deadline);

			if (err) {
				return err;
			}
			continue;
		}
		if (count <= 0) {
			return count == 0 ? -EIO : -errno;
		}
		offset += (size_t)count;
		if (packet && offset != size) {
			return -EIO;
		}
	}
	return 0;
}
