#include "support/test_support.h"
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
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
#include "host/hci_core.h"
#include "host/conn_internal.h"
#include "host/classic/l2cap_br_interface.h"

#define ECHO_DATA "ping"

#define L2CAP_HDR_SIZE 4U
#define SIG_HDR_SIZE   4U
#define CMD_REJECT     0x01U
#define ECHO_REQ       0x08U
#define ECHO_RSP       0x09U
#define INFO_RSP       0x0bU

/* No user data, so the L2CAP layer fails to send these buffers */
NET_BUF_POOL_FIXED_DEFINE(unsendable_pool, 3, BT_L2CAP_BR_ECHO_REQ_RESERVE + sizeof(ECHO_DATA), 0,
			  NULL);
NET_BUF_POOL_FIXED_DEFINE(tx_pool, 4, BT_L2CAP_BR_ECHO_REQ_RESERVE + sizeof(ECHO_DATA),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);
/* Two, as the stack keeps a received PDU it cannot process yet */
NET_BUF_POOL_FIXED_DEFINE(rx_pool, 2, 32, 0, NULL);

static os_sem_t worker_entered;
static os_sem_t worker_release;
static struct bt_work worker_blocker;

/* Keep the real Host worker paused while inspecting untransmitted PDUs. */
static void worker_block(struct bt_work *work)
{
	ARG_UNUSED(work);
	osi_sem_give(&worker_entered);
	osi_sem_take(&worker_release, BT_FOREVER);
}

static struct bt_conn *conn;
/* Identifier of the Information Request sent on connection, left unanswered */
static uint8_t info_ident;
static uint32_t echo_rsp_count;

static void echo_rsp_cb(struct bt_conn *rsp_conn, struct net_buf *buf)
{
	ARG_UNUSED(rsp_conn);
	ARG_UNUSED(buf);

	echo_rsp_count++;
}

static struct bt_l2cap_br_echo_cb echo_cb = {
	.rsp = echo_rsp_cb,
};

static struct bt_l2cap_br_chan *sig_chan(void)
{
	struct bt_l2cap_chan *sig;

	sig = bt_l2cap_br_lookup_rx_cid(conn, BT_L2CAP_CID_BR_SIG);
	assert_non_null(sig);

	return CONTAINER_OF(sig, struct bt_l2cap_br_chan, chan);
}

static struct net_buf *alloc_echo_buf(struct net_buf_pool *pool)
{
	struct net_buf *buf;

	buf = net_buf_alloc(pool, BT_NO_WAIT);
	assert_non_null(buf);

	net_buf_reserve(buf, BT_L2CAP_BR_ECHO_REQ_RESERVE);
	net_buf_add_mem(buf, ECHO_DATA, strlen(ECHO_DATA));

	return buf;
}

static struct net_buf *echo_buf(void)
{
	return alloc_echo_buf(&unsendable_pool);
}

/* Send an Echo Request and return its identifier, taken from the queued PDU */
static uint8_t send_echo_req(void)
{
	struct net_buf *buf = alloc_echo_buf(&tx_pool);
	int err;

	err = bt_l2cap_br_echo_req(conn, buf);
	assert_int_equal(err, 0);

	assert_ptr_equal(sys_slist_peek_tail(&sig_chan()->_pdu_tx_queue), &buf->node);
	assert_int_equal(buf->data[L2CAP_HDR_SIZE], ECHO_REQ);

	return buf->data[L2CAP_HDR_SIZE + 1U];
}

static struct net_buf *sig_pdu(uint8_t code, uint8_t ident, const void *data, uint16_t len)
{
	struct net_buf *buf;

	buf = net_buf_alloc(&rx_pool, BT_NO_WAIT);
	assert_non_null(buf);

	net_buf_add_le16(buf, SIG_HDR_SIZE + len);
	net_buf_add_le16(buf, BT_L2CAP_CID_BR_SIG);
	net_buf_add_u8(buf, code);
	net_buf_add_u8(buf, ident);
	net_buf_add_le16(buf, len);
	net_buf_add_mem(buf, data, len);

	return buf;
}

static void recv_sig(uint8_t code, uint8_t ident, const void *data, uint16_t len)
{
	bt_l2cap_br_recv(conn, sig_pdu(code, ident, data, len));
}

static void recv_echo_rsp(uint8_t ident)
{
	recv_sig(ECHO_RSP, ident, ECHO_DATA, strlen(ECHO_DATA));
}

static void check_unchanged(const struct net_buf *buf)
{
	assert_int_equal(net_buf_headroom(buf), BT_L2CAP_BR_ECHO_REQ_RESERVE);
	assert_int_equal(buf->len, strlen(ECHO_DATA));
	assert_memory_equal(buf->data, ECHO_DATA, strlen(ECHO_DATA));
}

static void *setup(void)
{
	static const bt_addr_t addr = { .val = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };

	conn = bt_conn_add_br(&addr);
	assert_non_null(conn);

	assert_int_equal(bt_l2cap_br_echo_cb_register(&echo_cb), 0);

	bt_l2cap_br_connected(conn);

	info_ident = sig_chan()->ident;
	assert_int_not_equal(info_ident, 0U);

	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	echo_rsp_count = 0U;
}

/* Release the PDUs queued for sending, as there is no controller */
static void release_tx_queue(void)
{
	sys_snode_t *node;

	while ((node = sys_slist_get(&sig_chan()->_pdu_tx_queue)) != NULL) {
		net_buf_unref(CONTAINER_OF(node, struct net_buf, node));
	}
}

static void after(void *fixture)
{
	ARG_UNUSED(fixture);

	release_tx_queue();
}

/* The last PDU queued for sending on the signaling channel, if any */
static struct net_buf *sig_tx_tail(void)
{
	sys_snode_t *node;

	node = sys_slist_peek_tail(&sig_chan()->_pdu_tx_queue);

	return (node != NULL) ? CONTAINER_OF(node, struct net_buf, node) : NULL;
}

static void check_echo_rsp_without_data(const struct net_buf *rsp, uint8_t ident)
{
	assert_non_null(rsp);
	assert_int_equal(rsp->len, L2CAP_HDR_SIZE + SIG_HDR_SIZE);
	assert_int_equal(rsp->data[L2CAP_HDR_SIZE], ECHO_RSP);
	assert_int_equal(rsp->data[L2CAP_HDR_SIZE + 1U], ident);
	assert_int_equal(sys_get_le16(&rsp->data[L2CAP_HDR_SIZE + 2U]), 0U);
}



/* A request that fails to send leaves the buffer unchanged */
static void l2cap_br_echo__test_echo_req_buf_unchanged(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf = echo_buf();
	int err;

	err = bt_l2cap_br_echo_req(conn, buf);
	assert_int_equal(err, -EINVAL);
	check_unchanged(buf);

	net_buf_unref(buf);
}

/* A request that fails to send does not wait for a response */
static void l2cap_br_echo__test_echo_req_not_pending(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf = echo_buf();
	int err;

	err = bt_l2cap_br_echo_req(conn, buf);
	assert_int_equal(err, -EINVAL);
	net_buf_unref(buf);

	/* -EBUSY would mean the first request is still waiting for a response */
	buf = echo_buf();
	err = bt_l2cap_br_echo_req(conn, buf);
	assert_int_equal(err, -EINVAL);
	net_buf_unref(buf);
}

/* A response that fails to send leaves the buffer unchanged */
static void l2cap_br_echo__test_echo_rsp_send_failure(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf = echo_buf();
	int err;

	err = bt_l2cap_br_echo_rsp(conn, 1, buf);
	assert_int_equal(err, -EINVAL);
	check_unchanged(buf);

	net_buf_unref(buf);
}

/* An Echo Request can be outstanding together with an Information Request */
static void l2cap_br_echo__test_echo_req_with_info_req(void **cm_state)
{
	(void)cm_state;
	static const uint8_t feat_mask_rsp[] = {
		/* Type: extended features mask, result: success, no features */
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	struct net_buf *buf;
	uint8_t echo_ident;
	int err;

	assert_int_equal(sig_chan()->ident, info_ident);

	echo_ident = send_echo_req();
	assert_int_not_equal(echo_ident, info_ident);

	/* The Information Response does not end the Echo Request */
	recv_sig(INFO_RSP, info_ident, feat_mask_rsp, sizeof(feat_mask_rsp));

	buf = alloc_echo_buf(&tx_pool);
	err = bt_l2cap_br_echo_req(conn, buf);
	assert_int_equal(err, -EBUSY);
	net_buf_unref(buf);

	recv_echo_rsp(echo_ident);
	assert_int_equal(echo_rsp_count, 1U);
}

/* An Echo Response with another identifier is ignored */
static void l2cap_br_echo__test_echo_rsp_ident_mismatch(void **cm_state)
{
	(void)cm_state;
	uint8_t echo_ident;

	echo_ident = send_echo_req();

	recv_echo_rsp((uint8_t)(echo_ident + 1U));
	assert_int_equal(echo_rsp_count, 0U);

	recv_echo_rsp(echo_ident);
	assert_int_equal(echo_rsp_count, 1U);

	/* The response ended the request, so another one can be sent */
	recv_echo_rsp(send_echo_req());
	assert_int_equal(echo_rsp_count, 2U);
}

/* A rejected Echo Request ends, and the signaling channel stays connected */
static void l2cap_br_echo__test_echo_req_rejected(void **cm_state)
{
	(void)cm_state;
	static const uint8_t reason[] = {0x00, 0x00};
	uint8_t echo_ident;

	echo_ident = send_echo_req();

	recv_sig(CMD_REJECT, echo_ident, reason, sizeof(reason));
	assert_int_equal(sig_chan()->state, BT_L2CAP_CONNECTED);

	recv_echo_rsp(send_echo_req());
	assert_int_equal(echo_rsp_count, 1U);
}

/* Without a req callback, the stack answers an Echo Request */
static void l2cap_br_echo__test_echo_req_answered_by_stack(void **cm_state)
{
	(void)cm_state;
	recv_sig(ECHO_REQ, 0x42U, ECHO_DATA, strlen(ECHO_DATA));

	check_echo_rsp_without_data(sig_tx_tail(), 0x42U);
}

/* An Echo Request received while out of TX buffers is kept, to be answered later */
static void l2cap_br_echo__test_echo_req_kept_without_buf(void **cm_state)
{
	(void)cm_state;
	struct net_buf *prev = NULL;
	struct net_buf *tail;
	struct net_buf *req;
	uint8_t ident;
	bool kept;

	/* Without a controller every response stays queued, until no buffer is left */
	for (ident = 1U; ident < UINT8_MAX; ident++) {
		req = net_buf_ref(sig_pdu(ECHO_REQ, ident, ECHO_DATA, strlen(ECHO_DATA)));
		bt_l2cap_br_recv(conn, req);
		kept = (req->ref > 1U);
		net_buf_unref(req);

		tail = sig_tx_tail();
		if (tail == prev) {
			break;
		}

		assert_false(kept);
		check_echo_rsp_without_data(tail, ident);
		prev = tail;
	}

	assert_true(ident < UINT8_MAX);
	/* Kept for the retry work, which needs the Bluetooth workqueue not run here */
	assert_true(kept);

	/* Drop the kept request with the signaling channel */
	release_tx_queue();
	bt_l2cap_br_disconnected(conn);
	bt_l2cap_br_connected(conn);
	info_ident = sig_chan()->ident;
	assert_int_not_equal(info_ident, 0U);
}

static void storage_init_bd3b1a7585(void)
{
	net_buf_pool_init(&unsendable_pool);
	net_buf_pool_init(&tx_pool);
	net_buf_pool_init(&rx_pool);
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&unsendable_pool);
	net_buf_pool_init(&tx_pool);
	net_buf_pool_init(&rx_pool);
}

int bt_test_l2cap_br_echo_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	bt_test_require(osi_sem_init(&worker_entered, 0, 1));
	bt_test_require(osi_sem_init(&worker_release, 0, 1));
	bt_work_init(&worker_blocker, worker_block);
	assert_true(bt_host_work_submit(&worker_blocker) >= 0);
	bt_test_require(osi_sem_take(&worker_entered, BT_SECONDS(1)));
	bt_test_require(bt_conn_init());
	migrated_pools_init();
	storage_init_bd3b1a7585();
	*state = setup();
	return 0;
}
static int bt_test_l2cap_br_echo_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}
int bt_test_l2cap_br_echo_after(void **state)
{
	(void)state;
	after(*state);
	return 0;
}
int bt_test_l2cap_br_echo_teardown(void **state)
{
	(void)state;
	bt_l2cap_br_disconnected(conn);
	bt_conn_unref(conn);
	conn = NULL;
	bt_test_require(osi_sem_give(&worker_release));
	assert_int_equal(bt_host_work_drain(), 0);
	bt_test_require(osi_sem_deinit(&worker_entered));
	bt_test_require(osi_sem_deinit(&worker_release));
	return 0;
}
static const struct CMUnitTest test_cases_l2cap_br_echo[] = {
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_req_buf_unchanged, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_req_not_pending, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_rsp_send_failure, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_req_with_info_req, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_rsp_ident_mismatch, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_req_rejected, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_req_answered_by_stack, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
	cmocka_unit_test_setup_teardown(l2cap_br_echo__test_echo_req_kept_without_buf, bt_test_l2cap_br_echo_before, bt_test_l2cap_br_echo_after),
};
BT_TEST_SUITE(l2cap_br_echo, test_cases_l2cap_br_echo, bt_test_l2cap_br_echo_setup, bt_test_l2cap_br_echo_teardown);

/* This fixture observes queued outbound PDUs without a running Controller.
 * The real connection queue operations run; the TX wakeup is captured here.
 */
static unsigned int captured_tx_wakeups;
void __wrap_bt_tx_irq_raise(void)
{
 captured_tx_wakeups++;
}
