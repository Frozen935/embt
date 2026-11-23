#include "support/test_support.h"
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/l2cap.h>

#include <embt/net_buf.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include "host/conn_internal.h"
#include "host/classic/l2cap_br_interface.h"

#define BASIC_CID  0x003eU
#define STREAM_CID 0x003fU
#define PAYLOAD    0xabcdU

struct test_chan {
	struct bt_l2cap_br_chan br;
	struct net_buf *kept;
	size_t recv_count;
	/* Whether recv() keeps the buffer and returns -EINPROGRESS */
	bool keep;
};

static struct test_chan basic_chan;
static struct test_chan stream_chan;
static struct bt_conn *conn;
static size_t freed;

static void pdu_destroy(struct net_buf *buf)
{
	freed++;
	net_buf_destroy(buf);
}

NET_BUF_POOL_FIXED_DEFINE(pdu_pool, 1, 16, 0, pdu_destroy);

static int test_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	struct test_chan *tc = CONTAINER_OF(chan, struct test_chan, br.chan);

	tc->recv_count++;
	assert_int_equal(buf->len, sizeof(uint16_t));
	assert_int_equal(sys_get_le16(buf->data), PAYLOAD);

	if (tc->keep) {
		tc->kept = buf;
		return -EINPROGRESS;
	}

	return 0;
}

static const struct bt_l2cap_chan_ops test_ops = {
	.recv = test_recv,
};

static int basic_accept(struct bt_conn *acl, struct bt_l2cap_chan **chan)
{
	basic_chan.br.chan.ops = &test_ops;
	*chan = &basic_chan.br.chan;

	return 0;
}

BT_L2CAP_BR_CHANNEL_DEFINE(test_basic_chan, BASIC_CID, basic_accept);

#if defined(CONFIG_BT_L2CAP_RET_FC)
static int stream_accept(struct bt_conn *acl, struct bt_l2cap_chan **chan)
{
	stream_chan.br.chan.ops = &test_ops;
	stream_chan.br.rx.mode = BT_L2CAP_BR_LINK_MODE_STREAM;
	stream_chan.br.rx.fcs = BT_L2CAP_BR_FCS_NO;
	stream_chan.br.rx.mps = 16;
	*chan = &stream_chan.br.chan;

	return 0;
}

BT_L2CAP_BR_CHANNEL_DEFINE(test_stream_chan, STREAM_CID, stream_accept);
#endif /* CONFIG_BT_L2CAP_RET_FC */

/* An L2CAP PDU carrying PAYLOAD, as an unsegmented I-frame when i_frame is set */
static struct net_buf *pdu(uint16_t cid, bool i_frame)
{
	struct net_buf *buf;
	uint16_t len = sizeof(uint16_t);

	buf = net_buf_alloc(&pdu_pool, BT_NO_WAIT);
	assert_non_null(buf);

	if (i_frame) {
		len += sizeof(uint16_t);
	}

	net_buf_add_le16(buf, len);
	net_buf_add_le16(buf, cid);

	if (i_frame) {
		/* TxSeq 0, ReqSeq 0, unsegmented */
		net_buf_add_le16(buf, 0U);
	}

	net_buf_add_le16(buf, PAYLOAD);

	return buf;
}

static void recv_kept(struct test_chan *tc, uint16_t cid, bool i_frame)
{
	tc->keep = true;
	bt_l2cap_br_recv(conn, pdu(cid, i_frame));

	assert_int_equal(tc->recv_count, 1);
	assert_non_null(tc->kept);
	assert_int_equal(freed, 0);
	assert_int_equal(sys_get_le16(tc->kept->data), PAYLOAD);

	/* Without CONFIG_BT_L2CAP_RET_FC this returns -ENOTSUP for BR/EDR, but it
	 * releases the buffer either way.
	 */
	(void)bt_l2cap_chan_recv_complete(&tc->br.chan, tc->kept);
	assert_int_equal(freed, 1);
}

static void recv_consumed(struct test_chan *tc, uint16_t cid, bool i_frame)
{
	bt_l2cap_br_recv(conn, pdu(cid, i_frame));

	assert_int_equal(tc->recv_count, 1);
	assert_int_equal(freed, 1);
}

static void *setup(void)
{
	static const bt_addr_t addr = { .val = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };

	conn = bt_conn_add_br(&addr);
	assert_non_null(conn);

	bt_l2cap_br_connected(conn);

	return NULL;
}

static void reset(struct test_chan *tc)
{
	tc->kept = NULL;
	tc->recv_count = 0;
	tc->keep = false;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	reset(&basic_chan);
	reset(&stream_chan);
	freed = 0;
}



static void l2cap_br_recv__test_basic_kept(void **cm_state)
{
	(void)cm_state;
	recv_kept(&basic_chan, BASIC_CID, false);
}

static void l2cap_br_recv__test_basic_consumed(void **cm_state)
{
	(void)cm_state;
	recv_consumed(&basic_chan, BASIC_CID, false);
}

static void l2cap_br_recv__test_stream_kept(void **cm_state)
{
	(void)cm_state;
	#if !defined(CONFIG_BT_L2CAP_RET_FC)
	skip();
#endif


	recv_kept(&stream_chan, STREAM_CID, true);
}

static void l2cap_br_recv__test_stream_consumed(void **cm_state)
{
	(void)cm_state;
	#if !defined(CONFIG_BT_L2CAP_RET_FC)
	skip();
#endif


	recv_consumed(&stream_chan, STREAM_CID, true);
}

static void storage_init_bb9f14f0e0(void)
{
	net_buf_pool_init(&pdu_pool);
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&pdu_pool);
}

int bt_test_l2cap_br_recv_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

 bt_test_require(bt_l2cap_br_fixed_chan_register(&test_basic_chan));
#if (defined(CONFIG_BT_L2CAP_RET_FC))
 bt_test_require(bt_l2cap_br_fixed_chan_register(&test_stream_chan));
#endif

	bt_test_require(bt_conn_init());
	migrated_pools_init();
	storage_init_bb9f14f0e0();
	*state = setup();
	return 0;
}
static int bt_test_l2cap_br_recv_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}


static const struct CMUnitTest test_cases_l2cap_br_recv[] = {
	cmocka_unit_test_setup_teardown(l2cap_br_recv__test_basic_kept, bt_test_l2cap_br_recv_before, NULL),
	cmocka_unit_test_setup_teardown(l2cap_br_recv__test_basic_consumed, bt_test_l2cap_br_recv_before, NULL),
	cmocka_unit_test_setup_teardown(l2cap_br_recv__test_stream_kept, bt_test_l2cap_br_recv_before, NULL),
	cmocka_unit_test_setup_teardown(l2cap_br_recv__test_stream_consumed, bt_test_l2cap_br_recv_before, NULL),
};
BT_TEST_SUITE(l2cap_br_recv, test_cases_l2cap_br_recv, bt_test_l2cap_br_recv_setup, NULL);

/* This fixture observes queued outbound PDUs without a running Controller.
 * The real connection queue operations run; the TX wakeup is captured here.
 */
static unsigned int captured_tx_wakeups;
void __wrap_bt_tx_irq_raise(void)
{
 captured_tx_wakeups++;
}
