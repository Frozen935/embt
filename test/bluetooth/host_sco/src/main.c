/*
 * SPDX-FileCopyrightText: Copyright 2026 NXP
 * SPDX-License-Identifier: Apache-2.0
 */

#include "support/test_support.h"
#include <errno.h>
#include <string.h>

#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci.h>
#include <embt/base/byteorder.h>

#include "common/hci_common_internal.h"
#include "host/conn_internal.h"
#include "host/classic/sco_internal.h"

BUILD_ASSERT(BT_BUF_CMD_TX_COUNT == 1, "The test requires exactly one command buffer");

static const bt_addr_t peer = {{1, 2, 3, 4, 5, 6}};
static struct bt_conn acl;
static struct bt_conn sco;
static struct bt_sco_chan chan;
static uint16_t expected_opcode;
static uint8_t expected_params[sizeof(struct bt_hci_cp_accept_sync_conn_req)];
static size_t expected_len;
static unsigned int hook_count;
static unsigned int send_count;
static unsigned int cleanup_count;
static int send_result;

struct bt_conn *__wrap_bt_conn_add_sco(const bt_addr_t *addr, int link_type)
{
	assert_memory_equal(addr, &peer, sizeof(peer));
	return &sco;
}

struct bt_conn *__wrap_bt_conn_lookup_addr_sco(const bt_addr_t *addr)
{
	assert_memory_equal(addr, &peer, sizeof(peer));
	return NULL;
}

void __wrap_bt_conn_set_state(struct bt_conn *conn, bt_conn_state_t state)
{
	assert_ptr_equal(conn, &sco);
	conn->state = state;
}

void __wrap_bt_conn_unref(struct bt_conn *conn)
{
	assert_ptr_equal(conn, &sco);
}

void __wrap_bt_sco_cleanup(struct bt_conn *conn)
{
	assert_ptr_equal(conn, &sco);
	cleanup_count++;
}

int __wrap_bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf, struct net_buf **rsp)
{
	assert_int_equal(opcode, expected_opcode);
	assert_int_equal(hook_count, 2);
	assert_int_equal(buf->len, expected_len);
	assert_memory_equal(buf->data, expected_params, expected_len);
	assert_null(rsp);
	send_count++;
	net_buf_unref(buf);
	return send_result;
}

static void check_command_buffer_available(void)
{
	struct net_buf *buf;

	/* Fail immediately rather than blocking if the caller holds the only buffer. */
	buf = bt_hci_cmd_alloc(BT_NO_WAIT);
	/* SCO hook must not run while its caller holds a command buffer. */
	assert_non_null(buf);
	net_buf_unref(buf);
	hook_count++;
}

static void setup_hook(struct bt_conn *conn, struct bt_hci_cp_setup_sync_conn *cp)
{
	struct bt_hci_cp_setup_sync_conn expected = {
		.handle = sys_cpu_to_le16(acl.handle),
		.tx_bandwidth = sys_cpu_to_le32(0x00001f40),
		.rx_bandwidth = sys_cpu_to_le32(0x00001f40),
		.max_latency = sys_cpu_to_le16(BT_HCI_SCO_MAX_LATENCY_DEFAULT),
		.content_format = sys_cpu_to_le16(chan.voice_setting),
		.retrans_effort = BT_HCI_SCO_RETRANS_EFFORT_DEFAULT,
		.pkt_type = sys_cpu_to_le16(sco.sco.pkt_type),
	};

	assert_ptr_equal(conn, &acl);
	assert_int_equal(hook_count, 0);
	assert_memory_equal(cp, &expected, sizeof(expected));
	check_command_buffer_available();
	cp->max_latency = sys_cpu_to_le16(0x1234);
}

static void accept_hook(struct bt_hci_cp_accept_sync_conn_req *cp)
{
	struct bt_hci_cp_accept_sync_conn_req expected = {
		.bdaddr = peer,
		.tx_bandwidth = sys_cpu_to_le32(0x00001f40),
		.rx_bandwidth = sys_cpu_to_le32(0x00001f40),
		.max_latency = sys_cpu_to_le16(BT_HCI_SCO_MAX_LATENCY_DEFAULT),
		.content_format = sys_cpu_to_le16(chan.voice_setting),
		.retrans_effort = BT_HCI_SCO_RETRANS_EFFORT_DEFAULT,
		.pkt_type = sys_cpu_to_le16(sco.sco.pkt_type),
	};

	assert_int_equal(hook_count, 0);
	assert_memory_equal(cp, &expected, sizeof(expected));
	check_command_buffer_available();
	cp->max_latency = sys_cpu_to_le16(0x1234);
}

static void static_setup_hook(struct bt_conn *conn, struct bt_hci_cp_setup_sync_conn *cp)
{
	assert_ptr_equal(conn, &acl);
	assert_int_equal(hook_count, 1);
	assert_int_equal(sys_le16_to_cpu(cp->max_latency), 0x1234);
	check_command_buffer_available();
	cp->pkt_type = sys_cpu_to_le16(0x0020);
	expected_len = sizeof(*cp);
	memcpy(expected_params, cp, expected_len);
}

static void static_accept_hook(struct bt_hci_cp_accept_sync_conn_req *cp)
{
	assert_int_equal(hook_count, 1);
	assert_int_equal(sys_le16_to_cpu(cp->max_latency), 0x1234);
	check_command_buffer_available();
	cp->pkt_type = sys_cpu_to_le16(0x0020);
	expected_len = sizeof(*cp);
	memcpy(expected_params, cp, expected_len);
}

static struct bt_sco_hci_cb sco_hooks = {
	.setup = static_setup_hook,
	.accept = static_accept_hook,
};

static struct bt_sco_hci_cb hooks = {
	.setup = setup_hook,
	.accept = accept_hook,
};

static int server_accept(const struct bt_sco_accept_info *info, struct bt_sco_chan **channel)
{
	assert_ptr_equal(info->acl, &acl);
	*channel = &chan;
	return 0;
}

static struct bt_sco_server server = {
	.sec_level = BT_SECURITY_L1,
	.accept = server_accept,
};

static int before(void **fixture)
{
	(void)fixture;
	static const struct bt_sco_chan_ops ops;

	memset(&acl, 0, sizeof(acl));
	memset(&sco, 0, sizeof(sco));
	memset(&chan, 0, sizeof(chan));
	acl.handle = 0x0123;
	acl.sec_level = BT_SECURITY_L1;
	sco.type = BT_CONN_TYPE_SCO;
	sco.sco.acl = &acl;
	sco.sco.pkt_type = 0x003f;
	chan.ops = &ops;
	chan.voice_setting = BT_HCI_VOICE_SETTINGS(
		BT_HCI_VOICE_SETTING_AIR_CODING_FMT_CVSD,
		BT_HCI_VOICE_SETTING_PCM_BIT_POS_DEFAULT,
		BT_HCI_VOICE_SETTING_SAMPLE_SIZE_16_BITS,
		BT_HCI_VOICE_SETTING_DATA_FMT_2_COMPLEMENT,
		BT_HCI_VOICE_SETTING_CODING_FMT_LINEAR);
	hook_count = 0;
	send_count = 0;
	cleanup_count = 0;
	send_result = 0;
	expected_len = 0;
	assert_int_equal(bt_sco_hci_cb_register(&hooks), 0);
	assert_int_equal(bt_sco_hci_cb_register(&sco_hooks), 0);
	assert_int_equal(bt_sco_server_register(&server), 0);
	return 0;
}

static int after(void **fixture)
{
	(void)fixture;
	assert_int_equal(bt_sco_hci_cb_unregister(&sco_hooks), 0);
	assert_int_equal(bt_sco_hci_cb_unregister(&hooks), 0);
	assert_int_equal(bt_sco_server_unregister(&server), 0);
	assert_int_equal(send_count, 1);
	assert_int_equal(hook_count, 2);
	check_command_buffer_available();
	return 0;
}

static uint8_t accept_connection(void)
{
	struct bt_hci_evt_conn_request evt = {
		.bdaddr = peer,
		.link_type = BT_HCI_ESCO,
	};

	expected_opcode = BT_HCI_OP_ACCEPT_SYNC_CONN_REQ;
	return bt_esco_conn_req(&evt);
}

static void sco_hooks__test_setup(void **state)
{
	(void)state;
	expected_opcode = BT_HCI_OP_SETUP_SYNC_CONN;
	assert_ptr_equal(bt_conn_create_sco(&peer, &chan), &sco);
	assert_int_equal(chan.state, BT_SCO_STATE_CONNECTING);
	assert_int_equal(cleanup_count, 0);
}

static void sco_hooks__test_accept(void **state)
{
	(void)state;
	assert_int_equal(accept_connection(), BT_HCI_ERR_SUCCESS);
	assert_int_equal(chan.state, BT_SCO_STATE_CONNECTING);
	assert_int_equal(cleanup_count, 0);
}

static void sco_hooks__test_setup_send_error(void **state)
{
	(void)state;
	expected_opcode = BT_HCI_OP_SETUP_SYNC_CONN;
	send_result = -EIO;
	assert_null(bt_conn_create_sco(&peer, &chan));
	assert_int_equal(chan.state, BT_SCO_STATE_DISCONNECTED);
	assert_int_equal(cleanup_count, 1);
}

static void sco_hooks__test_accept_send_error(void **state)
{
	(void)state;
	send_result = -EIO;
	assert_int_equal(accept_connection(), BT_HCI_ERR_UNSPECIFIED);
	assert_int_equal(chan.state, BT_SCO_STATE_DISCONNECTED);
	assert_null(chan.sco);
	assert_int_equal(cleanup_count, 1);
}

static const struct CMUnitTest cases[] = {
	cmocka_unit_test_setup_teardown(sco_hooks__test_setup, before, after),
	cmocka_unit_test_setup_teardown(sco_hooks__test_accept, before, after),
	cmocka_unit_test_setup_teardown(sco_hooks__test_setup_send_error, before, after),
	cmocka_unit_test_setup_teardown(sco_hooks__test_accept_send_error, before, after),
};
BT_TEST_SUITE(sco_hooks, cases, bt_test_host_setup, NULL);
