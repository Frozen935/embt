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
 * @brief Real TCP/Unix transport and modeled Linux HCI user channel lifecycle tests.
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <base/base.h>
#include <embt/drivers/bluetooth.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/transport.h>
#include <host/hci_core.h>
#include "socket_io.h"
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static struct k_sem received;
static unsigned int receive_count;
static uint8_t last_frame[64];
static size_t last_size;
static bool user_model;
static bool fail_binding;
static bool fail_device_down;
static bool device_down;
static int user_peer = -1;
static int user_fd = -1;
static unsigned short bound_channel;
static unsigned short bound_controller;

int __real_socket(int domain, int type, int protocol);
int __real_bind(int fd, const struct sockaddr *address, socklen_t length);
int __real_ioctl(int fd, unsigned long request, ...);

/** @brief Model only AF_BLUETOOTH sockets; TCP and Unix fixtures use the real kernel. */
int __wrap_socket(int domain, int type, int protocol)
{
	if (domain == AF_BLUETOOTH && user_model) {
		int pair[2];

		assert_int_equal(type & 0xf, SOCK_RAW);
		assert_int_equal(protocol, 1);
		assert_int_equal(
			socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, pair),
			0);
		device_down = false;
		user_fd = pair[0];
		user_peer = pair[1];
		return user_fd;
	}
	return __real_socket(domain, type, protocol);
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
	va_list args;
	unsigned long controller;

	va_start(args, request);
	controller = va_arg(args, unsigned long);
	va_end(args);
	if (fd == user_fd && user_model) {
		assert_int_equal(request, _IOW('H', 202, int));
		bound_controller = (unsigned short)controller;
		if (fail_device_down) {
			errno = EPERM;
			return -1;
		}
		device_down = true;
		return 0;
	}
	return __real_ioctl(fd, request, controller);
}

int __wrap_bind(int fd, const struct sockaddr *address, socklen_t length)
{
	if (fd == user_fd && user_model) {
		struct hci_address {
			sa_family_t family;
			unsigned short controller;
			unsigned short channel;
		} addr;

		assert_true(device_down);
		assert_int_equal(length, sizeof(addr));
		memcpy(&addr, address, sizeof(addr));
		assert_int_equal(addr.family, AF_BLUETOOTH);
		bound_channel = addr.channel;
		bound_controller = addr.controller;
		if (fail_binding) {
			errno = EBUSY;
			return -1;
		}
		return 0;
	}
	return __real_bind(fd, address, length);
}

static int receive_frame(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	(void)dev;
	last_size = MIN(buf->len, sizeof(last_frame));
	memcpy(last_frame, buf->data, last_size);
	receive_count++;
	net_buf_unref(buf);
	k_sem_give(&received);
	return 0;
}

struct endpoint {
	struct bt_hci_socket_config config;
	int listener;
	int peer;
	char path[108];
};

static void open_endpoint(struct endpoint *endpoint, enum bt_hci_socket_kind kind)
{
	memset(endpoint, 0, sizeof(*endpoint));
	endpoint->listener = -1;
	endpoint->peer = -1;
	endpoint->config.kind = kind;
	if (kind == BT_HCI_SOCKET_TCP) {
		struct sockaddr_in address = {.sin_family = AF_INET,
					      .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
		socklen_t size = sizeof(address);

		endpoint->listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
		assert_true(endpoint->listener >= 0);
		assert_int_equal(
			bind(endpoint->listener, (struct sockaddr *)&address, sizeof(address)), 0);
		assert_int_equal(
			getsockname(endpoint->listener, (struct sockaddr *)&address, &size), 0);
		endpoint->config.address = "127.0.0.1";
		endpoint->config.port = ntohs(address.sin_port);
	} else if (kind == BT_HCI_SOCKET_UNIX) {
		struct sockaddr_un address = {.sun_family = AF_UNIX};

		snprintf(endpoint->path, sizeof(endpoint->path), "/tmp/embt-transport-%ld.sock",
			 (long)getpid());
		strcpy(address.sun_path, endpoint->path);
		endpoint->listener = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		assert_true(endpoint->listener >= 0);
		assert_int_equal(
			bind(endpoint->listener, (struct sockaddr *)&address, sizeof(address)), 0);
		endpoint->config.address = endpoint->path;
	} else {
		user_model = true;
		endpoint->config.controller = 7;
	}
	if (endpoint->listener >= 0) {
		assert_int_equal(listen(endpoint->listener, 1), 0);
	}
	assert_int_equal(bt_hci_socket_configure(&endpoint->config), 0);
	assert_int_equal(bt_hci_open(bt_dev.hci, receive_frame), 0);
	if (endpoint->listener >= 0) {
		endpoint->peer = accept(endpoint->listener, NULL, NULL);
		assert_true(endpoint->peer >= 0);
	} else {
		endpoint->peer = user_peer;
		assert_int_equal(bound_channel, 1);
		assert_int_equal(bound_controller, 7);
	}
}

static void close_endpoint(struct endpoint *endpoint)
{
	assert_int_equal(bt_hci_close(bt_dev.hci), 0);
	close(endpoint->peer);
	if (endpoint->listener >= 0) {
		close(endpoint->listener);
	}
	if (endpoint->path[0]) {
		unlink(endpoint->path);
	}
}

static const uint8_t event[] = {0x04, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
NET_BUF_POOL_FIXED_DEFINE(tx_pool, 2, 64, 0, NULL);

static void exercise_transport(enum bt_hci_socket_kind kind)
{
	struct endpoint endpoint;
	struct net_buf *buf;
	uint8_t readback[16];
	uint8_t joined[sizeof(event) * 2];
	const uint8_t command[] = {0x01, 0x03, 0x0c, 0x00};
	struct pollfd input;

	open_endpoint(&endpoint, kind);
	if (kind == BT_HCI_SOCKET_USER) {
		assert_int_equal(send(endpoint.peer, event, sizeof(event), MSG_NOSIGNAL),
				 sizeof(event));
	} else {
		assert_int_equal(send(endpoint.peer, event, 1, MSG_NOSIGNAL), 1);
		assert_int_equal(k_sem_take(&received, K_MSEC(10)), -EAGAIN);
		assert_int_equal(send(endpoint.peer, event + 1, sizeof(event) - 1, MSG_NOSIGNAL),
				 sizeof(event) - 1);
	}
	assert_int_equal(k_sem_take(&received, K_MSEC(1000)), 0);
	assert_int_equal(last_size, sizeof(event));
	assert_memory_equal(last_frame, event, sizeof(event));
	if (kind != BT_HCI_SOCKET_USER) {
		memcpy(joined, event, sizeof(event));
		memcpy(joined + sizeof(event), event, sizeof(event));
		assert_int_equal(send(endpoint.peer, joined, sizeof(joined), MSG_NOSIGNAL),
				 sizeof(joined));
		assert_int_equal(k_sem_take(&received, K_MSEC(1000)), 0);
		assert_int_equal(k_sem_take(&received, K_MSEC(1000)), 0);
		assert_int_equal(receive_count, 3);
	}
	buf = net_buf_alloc(&tx_pool, K_NO_WAIT);
	assert_non_null(buf);
	net_buf_add_mem(buf, command, sizeof(command));
	assert_int_equal(bt_hci_send(bt_dev.hci, buf), 0);
	input = (struct pollfd){.fd = endpoint.peer, .events = POLLIN};
	assert_int_equal(poll(&input, 1, 1000), 1);
	assert_int_equal(recv(endpoint.peer, readback, sizeof(readback), 0), sizeof(command));
	assert_memory_equal(readback, command, sizeof(command));
	assert_int_equal(bt_hci_open(bt_dev.hci, receive_frame), -EALREADY);
	close_endpoint(&endpoint);
	assert_null(((struct bt_hci_driver_data *)bt_dev.hci->data)->recv);
}

static void test_tcp(void **state)
{
	(void)state;
	exercise_transport(BT_HCI_SOCKET_TCP);
}
static void test_unix(void **state)
{
	(void)state;
	exercise_transport(BT_HCI_SOCKET_UNIX);
}
static void test_user_model(void **state)
{
	(void)state;
	exercise_transport(BT_HCI_SOCKET_USER);
}

static void test_close_exhausted(void **state)
{
	struct endpoint endpoint;
	struct net_buf *held;
	int64_t start;

	(void)state;
	open_endpoint(&endpoint, BT_HCI_SOCKET_TCP);
	held = bt_buf_get_evt(0x0e, false, K_NO_WAIT);
	assert_non_null(held);
	assert_int_equal(send(endpoint.peer, event, sizeof(event), MSG_NOSIGNAL), sizeof(event));
	assert_int_equal(k_sem_take(&received, K_MSEC(20)), -EAGAIN);
	start = k_uptime_get();
	close_endpoint(&endpoint);
	assert_in_range(k_uptime_get() - start, 0, 500);
	net_buf_unref(held);
	assert_int_equal(receive_count, 0);
}

static void test_bad_packet(void **state)
{
	struct endpoint endpoint;
	struct net_buf *buf;
	const uint8_t invalid[] = {0x04, 0x0e, 0x04, 0x01};

	(void)state;
	open_endpoint(&endpoint, BT_HCI_SOCKET_USER);
	assert_int_equal(send(endpoint.peer, invalid, sizeof(invalid), MSG_NOSIGNAL),
			 sizeof(invalid));
	assert_int_equal(k_sem_take(&received, K_MSEC(30)), -EAGAIN);
	buf = net_buf_alloc(&tx_pool, K_NO_WAIT);
	assert_non_null(buf);
	net_buf_add_u8(buf, 1);
	assert_int_equal(bt_hci_send(bt_dev.hci, buf), -EPROTO);
	assert_int_equal(buf->ref, 1);
	net_buf_unref(buf);
	close_endpoint(&endpoint);
}

static void test_device_down_failure(void **state)
{
	struct bt_hci_socket_config config = {.kind = BT_HCI_SOCKET_USER, .controller = 6};

	(void)state;
	user_model = true;
	fail_device_down = true;
	assert_int_equal(bt_hci_socket_configure(&config), 0);
	assert_int_equal(bt_hci_open(bt_dev.hci, receive_frame), -EPERM);
	assert_false(device_down);
	assert_null(((struct bt_hci_driver_data *)bt_dev.hci->data)->recv);
	assert_int_equal(fcntl(user_fd, F_GETFD), -1);
	assert_int_equal(errno, EBADF);
	close(user_peer);
	fail_device_down = false;
	assert_int_equal(bt_hci_open(bt_dev.hci, receive_frame), 0);
	assert_true(device_down);
	assert_int_equal(bound_controller, 6);
	assert_int_equal(bt_hci_close(bt_dev.hci), 0);
	close(user_peer);
}

static void test_open_failure(void **state)
{
	struct bt_hci_socket_config config = {.kind = BT_HCI_SOCKET_USER, .controller = 5};

	(void)state;
	user_model = true;
	fail_binding = true;
	assert_int_equal(bt_hci_socket_configure(&config), 0);
	assert_int_equal(bt_hci_open(bt_dev.hci, receive_frame), -EBUSY);
	assert_null(((struct bt_hci_driver_data *)bt_dev.hci->data)->recv);
	assert_int_equal(fcntl(user_fd, F_GETFD), -1);
	assert_int_equal(errno, EBADF);
	close(user_peer);
	fail_binding = false;
	assert_int_equal(bt_hci_open(bt_dev.hci, receive_frame), 0);
	assert_int_equal(bt_hci_close(bt_dev.hci), 0);
	close(user_peer);
}

struct send_context {
	int fd;
	int stop;
	const uint8_t *data;
	size_t size;
	int result;
};
static void send_bytes(void *arg)
{
	struct send_context *ctx = arg;

	ctx->result = bt_socket_write(ctx->fd, ctx->stop, ctx->data, ctx->size, 0);
}

static void test_stream_backpressure(void **state)
{
	int pair[2];
	int capacity = 4096;
	uint8_t *data = osi_malloc(512 * 1024);
	uint8_t block[997];
	size_t offset = 0;
	os_thread_t sender;
	struct send_context ctx;

	(void)state;
	assert_non_null(data);
	for (size_t i = 0; i < 512 * 1024; i++) {
		data[i] = (uint8_t)i;
	}
	assert_int_equal(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, pair), 0);
	assert_int_equal(setsockopt(pair[0], SOL_SOCKET, SO_SNDBUF, &capacity, sizeof(capacity)),
			 0);
	ctx = (struct send_context){.fd = pair[0], .stop = -1, .data = data, .size = 512 * 1024};
	assert_int_equal(osi_thread_create(&sender, send_bytes, &ctx, "stream-send", 0, 0), 0);
	while (offset < ctx.size) {
		struct pollfd input = {.fd = pair[1], .events = POLLIN};
		ssize_t count;

		assert_int_equal(poll(&input, 1, 1000), 1);
		count = recv(pair[1], block, sizeof(block), 0);
		assert_true(count > 0);
		assert_memory_equal(block, data + offset, (size_t)count);
		offset += (size_t)count;
	}
	assert_int_equal(osi_thread_join(&sender), 0);
	assert_int_equal(ctx.result, 0);
	ctx.stop = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	assert_true(ctx.stop >= 0);
	assert_int_equal(osi_thread_create(&sender, send_bytes, &ctx, "stream-stop", 0, 0), 0);
	osi_sleep_ms(10);
	uint64_t wake = 1;

	assert_int_equal(write(ctx.stop, &wake, sizeof(wake)), sizeof(wake));
	assert_int_equal(osi_thread_join(&sender), 0);
	assert_int_equal(ctx.result, -ECANCELED);
	close(pair[0]);
	close(pair[1]);
	close(ctx.stop);
	osi_free(data);
}

static int setup(void **state)
{
	(void)state;
	int err = bt_base_init();

	if (!err) {
		err = k_sem_init(&received, 0, 10);
	}
	return err;
}

static int teardown(void **state)
{
	(void)state;
	return bt_base_deinit();
}

int main(int argc, char **argv)
{
	const struct {
		const char *name;
		void (*fn)(void **);
	} cases[] = {
		{"tcp", test_tcp},
		{"unix", test_unix},
		{"user_model", test_user_model},
		{"close_exhausted", test_close_exhausted},
		{"bad_packet", test_bad_packet},
		{"open_failure", test_open_failure},
		{"device_down_failure", test_device_down_failure},
		{"stream_backpressure", test_stream_backpressure},
	};

	if (argc != 2) {
		return 2;
	}
	for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
		if (!strcmp(argv[1], cases[i].name)) {
			const struct CMUnitTest tests[] = {
				{cases[i].name, cases[i].fn, setup, teardown, NULL}};

			return cmocka_run_group_tests(tests, NULL, NULL);
		}
	}
	return 2;
}
