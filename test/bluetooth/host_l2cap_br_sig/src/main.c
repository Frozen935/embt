#include "support/test_support.h"
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/classic/l2cap_br.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/l2cap.h>

#include <embt/net_buf.h>
#include <embt/base/byteorder.h>
#include <embt/base/slist.h>
#include <embt/base/util.h>
#include "host/conn_internal.h"
#include "host/classic/l2cap_br_interface.h"

#define SIG_CID      0x0001U
#define CONNLESS_CID 0x0002U
#define ECHO_REQ     0x08U
#define ECHO_RSP     0x09U
#define CONNLESS_PSM 0x1001U
#define MAX_CALLS    2
#define MAX_DATA     8

/* What one callback invocation saw */
struct seen {
	uint8_t ident;
	uint16_t len;
	uint8_t data[MAX_DATA];
};

/* Calls seen by each of two registered callbacks */
struct cb_log {
	struct seen calls[MAX_CALLS];
	size_t count;
};

static struct cb_log echo_log[2];
static struct cb_log echo_rsp_log[2];
static struct cb_log connless_log[2];
static struct bt_conn *conn;

NET_BUF_POOL_FIXED_DEFINE(pdu_pool, 1, 32, 0, NULL);
NET_BUF_POOL_FIXED_DEFINE(tx_pool, 1, BT_L2CAP_BR_ECHO_REQ_RESERVE + MAX_DATA,
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

/* Records the data and then consumes it, as a callback parsing it would */
static void record(struct cb_log *rec, uint8_t ident, struct net_buf *buf)
{
	struct seen *s;

	assert_true(rec->count < ARRAY_SIZE(rec->calls));
	assert_true(buf->len <= sizeof(s->data));

	s = &rec->calls[rec->count++];
	s->ident = ident;
	s->len = buf->len;
	memcpy(s->data, buf->data, buf->len);

	(void)net_buf_pull(buf, buf->len);
}

static void check(const struct cb_log *rec, size_t call, uint8_t ident, const char *data)
{
	const struct seen *s = &rec->calls[call];

	assert_int_equal(s->ident, ident);
	assert_int_equal(s->len, strlen(data));
	assert_memory_equal(s->data, data, strlen(data));
}

static void echo_req_0(struct bt_conn *acl, uint8_t ident, struct net_buf *buf)
{
	record(&echo_log[0], ident, buf);
}

static void echo_req_1(struct bt_conn *acl, uint8_t ident, struct net_buf *buf)
{
	record(&echo_log[1], ident, buf);
}

static void echo_rsp_0(struct bt_conn *acl, struct net_buf *buf)
{
	record(&echo_rsp_log[0], 0, buf);
}

static void echo_rsp_1(struct bt_conn *acl, struct net_buf *buf)
{
	record(&echo_rsp_log[1], 0, buf);
}

static struct bt_l2cap_br_echo_cb echo_cb[] = {
	{ .req = echo_req_0, .rsp = echo_rsp_0 },
	{ .req = echo_req_1, .rsp = echo_rsp_1 },
};

static void connless_recv_0(struct bt_conn *acl, uint16_t psm, struct net_buf *buf)
{
	assert_int_equal(psm, CONNLESS_PSM);
	record(&connless_log[0], 0, buf);
}

static void connless_recv_1(struct bt_conn *acl, uint16_t psm, struct net_buf *buf)
{
	assert_int_equal(psm, CONNLESS_PSM);
	record(&connless_log[1], 0, buf);
}

/* One callback for all PSMs and one for CONNLESS_PSM, so both get the frame */
static struct bt_l2cap_br_connless_cb connless_cb[] = {
	{ .psm = 0, .sec_level = BT_SECURITY_L0, .recv = connless_recv_0 },
	{ .psm = CONNLESS_PSM, .sec_level = BT_SECURITY_L0, .recv = connless_recv_1 },
};

static struct net_buf *l2cap_pdu(uint16_t cid)
{
	struct net_buf *buf;

	buf = net_buf_alloc(&pdu_pool, BT_NO_WAIT);
	assert_non_null(buf);

	/* Length, filled in by send_pdu() */
	net_buf_add_le16(buf, 0U);
	net_buf_add_le16(buf, cid);

	return buf;
}

static void send_pdu(struct net_buf *buf)
{
	sys_put_le16(buf->len - 4U, buf->data);
	bt_l2cap_br_recv(conn, buf);
}

static void add_sig_cmd(struct net_buf *buf, uint8_t code, uint8_t ident, const char *data)
{
	net_buf_add_u8(buf, code);
	net_buf_add_u8(buf, ident);
	net_buf_add_le16(buf, strlen(data));
	net_buf_add_mem(buf, data, strlen(data));
}

static struct bt_l2cap_br_chan *sig_chan(void)
{
	struct bt_l2cap_chan *sig = bt_l2cap_br_lookup_rx_cid(conn, SIG_CID);

	assert_non_null(sig);

	return CONTAINER_OF(sig, struct bt_l2cap_br_chan, chan);
}

/* Send an Echo Request and return its identifier, taken from the queued PDU */
static uint8_t send_echo_req(void)
{
	struct net_buf *buf;
	int err;

	buf = net_buf_alloc(&tx_pool, BT_NO_WAIT);
	assert_non_null(buf);
	net_buf_reserve(buf, BT_L2CAP_BR_ECHO_REQ_RESERVE);

	err = bt_l2cap_br_echo_req(conn, buf);
	assert_int_equal(err, 0);
	assert_ptr_equal(sys_slist_peek_tail(&sig_chan()->_pdu_tx_queue), &buf->node);

	/* L2CAP header, then the signaling header code and identifier */
	return buf->data[5];
}

static void *setup(void)
{
	static const bt_addr_t addr = { .val = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };

	ARRAY_FOR_EACH_PTR(echo_cb, cb) {
		assert_int_equal(bt_l2cap_br_echo_cb_register(cb), 0);
	}

	ARRAY_FOR_EACH_PTR(connless_cb, cb) {
		assert_int_equal(bt_l2cap_br_connless_register(cb), 0);
	}

	conn = bt_conn_add_br(&addr);
	assert_non_null(conn);

	bt_l2cap_br_connected(conn);

	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	memset(echo_log, 0, sizeof(echo_log));
	memset(echo_rsp_log, 0, sizeof(echo_rsp_log));
	memset(connless_log, 0, sizeof(connless_log));
}

/* Release the PDUs queued for sending, as there is no controller */
static void after(void *fixture)
{
	sys_snode_t *node;

	ARG_UNUSED(fixture);

	while ((node = sys_slist_get(&sig_chan()->_pdu_tx_queue)) != NULL) {
		net_buf_unref(CONTAINER_OF(node, struct net_buf, node));
	}
}



/* Each Echo Request callback sees exactly the data of that request, although
 * another request follows in the same signaling packet and the other callback
 * consumes the data.
 */
static void l2cap_br_sig__test_echo_req(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf = l2cap_pdu(SIG_CID);

	add_sig_cmd(buf, ECHO_REQ, 1, "abc");
	add_sig_cmd(buf, ECHO_REQ, 2, "xy");
	send_pdu(buf);

	ARRAY_FOR_EACH_PTR(echo_log, rec) {
		assert_int_equal(rec->count, 2);
		check(rec, 0, 1, "abc");
		check(rec, 1, 2, "xy");
	}
}

/* Each Echo Response callback sees exactly the data of the response */
static void l2cap_br_sig__test_echo_rsp(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf;
	uint8_t ident;

	ident = send_echo_req();

	buf = l2cap_pdu(SIG_CID);
	add_sig_cmd(buf, ECHO_RSP, ident, "pq");
	add_sig_cmd(buf, ECHO_REQ, (uint8_t)(ident + 1U), "z");
	send_pdu(buf);

	ARRAY_FOR_EACH_PTR(echo_rsp_log, rec) {
		assert_int_equal(rec->count, 1);
		check(rec, 0, 0, "pq");
	}
}

/* Each connectionless callback sees the whole data, although the other one consumes it */
static void l2cap_br_sig__test_connless_recv(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf = l2cap_pdu(CONNLESS_CID);

	net_buf_add_le16(buf, CONNLESS_PSM);
	net_buf_add_mem(buf, "hello", strlen("hello"));
	send_pdu(buf);

	ARRAY_FOR_EACH_PTR(connless_log, rec) {
		assert_int_equal(rec->count, 1);
		check(rec, 0, 0, "hello");
	}
}

static void storage_init_b47e028381(void)
{
	net_buf_pool_init(&pdu_pool);
	net_buf_pool_init(&tx_pool);
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&pdu_pool);
	net_buf_pool_init(&tx_pool);
}

int bt_test_l2cap_br_sig_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	bt_test_require(bt_conn_init());
	migrated_pools_init();
	storage_init_b47e028381();
	*state = setup();
	return 0;
}
static int bt_test_l2cap_br_sig_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}
int bt_test_l2cap_br_sig_after(void **state)
{
	(void)state;
	after(*state);
	return 0;
}

static const struct CMUnitTest test_cases_l2cap_br_sig[] = {
	cmocka_unit_test_setup_teardown(l2cap_br_sig__test_echo_req, bt_test_l2cap_br_sig_before, bt_test_l2cap_br_sig_after),
	cmocka_unit_test_setup_teardown(l2cap_br_sig__test_echo_rsp, bt_test_l2cap_br_sig_before, bt_test_l2cap_br_sig_after),
	cmocka_unit_test_setup_teardown(l2cap_br_sig__test_connless_recv, bt_test_l2cap_br_sig_before, bt_test_l2cap_br_sig_after),
};
BT_TEST_SUITE(l2cap_br_sig, test_cases_l2cap_br_sig, bt_test_l2cap_br_sig_setup, NULL);

/* This fixture observes queued outbound PDUs without a running Controller.
 * The real connection queue operations run; the TX wakeup is captured here.
 */
static unsigned int captured_tx_wakeups;
void __wrap_bt_tx_irq_raise(void)
{
 captured_tx_wakeups++;
}
