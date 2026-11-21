#include "support/test_support.h"
/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/hci_pkt.h>
#include <embt/bluetooth/hci_types.h>
#include "common/hci_lockstep.h"

#include <embt/net_buf.h>
#define TEST_OPCODE 0xfc01U

/* Fake transport for the lockstep tests: records the sent packet and can
 * deliver a canned response, either synchronously from within send (as a
 * request/response transport does) or from a delayed work item.
 */
struct fake_transport {
	bool initialized;
	struct bt_hci_transport dev;
	struct bt_hci_lockstep ls;
	uint8_t sent[BT_HCI_PKT_CMD_SIZE(UINT8_MAX)];
	size_t sent_len;
	int send_err;
	/* Optional packet delivered before the response, e.g. an unrelated
	 * or malformed event; its fate is recorded for the test to check.
	 */
	const uint8_t *pre;
	size_t pre_len;
	bool pre_consumed;
	const uint8_t *rsp;
	size_t rsp_len;
	bool rsp_in_send;
	/* Reset the helper from within send, before the response is fed */
	bool reset_in_send;
	/* Reset the helper from the work item instead of feeding from it */
	bool reset_from_work;
	unsigned int sends;
	struct bt_work_delayable rsp_work;
};

static void feed_canned_rsp(struct fake_transport *xport)
{
	if (xport->pre != NULL) {
		xport->pre_consumed = bt_hci_lockstep_feed(&xport->ls, xport->pre, xport->pre_len);
	}

	/* A driver feeds the packet it received, from its own buffer */
	(void)bt_hci_lockstep_feed(&xport->ls, xport->rsp, xport->rsp_len);
}





static int fake_send(const struct bt_hci_transport *dev, const uint8_t *pkt, size_t len)
{
	struct fake_transport *xport = dev->data;

	assert_true(len <= sizeof(xport->sent));
	(void)memcpy(xport->sent, pkt, len);
	xport->sent_len = len;
	xport->sends++;

	if (xport->reset_in_send) {
		bt_hci_lockstep_reset(&xport->ls);
	}

	/* A transport may deliver packets before it reports a failure */
	if (xport->rsp != NULL && xport->rsp_in_send) {
		feed_canned_rsp(xport);
	}

	return xport->send_err;
}

static void rsp_work_handler(struct bt_work *work)
{
	struct bt_work_delayable *dwork = bt_work_delayable_from_work(work);
	struct fake_transport *xport = CONTAINER_OF(dwork, struct fake_transport, rsp_work);

	if (xport->reset_from_work) {
		bt_hci_lockstep_reset(&xport->ls);
	} else {
		feed_canned_rsp(xport);
	}
}

static struct fake_transport *owned_transports[8];
static size_t owned_count;

static void clear_transport(struct fake_transport *xport)
{
	if (xport->initialized) {
		struct bt_work_sync sync = {0};

		bt_work_cancel_delayable_sync(&xport->rsp_work, &sync);
		bt_test_require(osi_sem_deinit(&xport->ls.changed));
		xport->initialized = false;
	}
}

static struct fake_transport *new_transport(void)
{
	assert_true(owned_count < ARRAY_SIZE(owned_transports));
	struct fake_transport *xport = calloc(1, sizeof(*xport));

	assert_non_null(xport);
	owned_transports[owned_count++] = xport;
	return xport;
}

static void fake_transport_init(struct fake_transport *xport)
{
	clear_transport(xport);
	(void)memset(xport, 0, sizeof(*xport));
	xport->dev.name = "fake_hci";
	xport->dev.data = xport;
	bt_work_init_delayable(&xport->rsp_work, rsp_work_handler);
	bt_hci_lockstep_init(&xport->ls, &xport->dev, fake_send);
	xport->initialized = true;
	/* Keep the timeout test short */
	xport->ls.timeout = BT_MSEC(100);
}

static const uint8_t cc_ok_rsp[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x06, 0x01, 0x01, 0xfc, 0x00, 0xaa, 0xbb
};
static const uint8_t cc_err_rsp[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x04, 0x01, 0x01, 0xfc, BT_HCI_ERR_UNSPECIFIED
};
static const uint8_t cc_other_rsp[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x04, 0x01, 0x02, 0xfc, 0x00
};
static const uint8_t cs_ok_rsp[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_STATUS, 0x04, 0x00, 0x01, 0x01, 0xfc
};
/* Responses whose Num_HCI_Command_Packets allows no further command */
static const uint8_t cc_ok_no_credit_rsp[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x06, 0x00, 0x01, 0xfc, 0x00, 0xaa, 0xbb
};
static const uint8_t cs_ok_no_credit_rsp[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_STATUS, 0x04, 0x00, 0x00, 0x01, 0xfc
};
/* The NOP Command Complete a controller sends to allow one command, and
 * one allowing none.
 */
static const uint8_t nop_cc_grant[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x03, 0x01, 0x00, 0x00
};
static const uint8_t nop_cc_revoke[] = {
	BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x03, 0x00, 0x00, 0x00
};

static void send_test_cmd(struct fake_transport *xport, struct net_buf_simple *cmd,
			  struct net_buf_simple *rsp, int expected_err)
{
	const uint8_t expected_pkt[] = { BT_HCI_H4_CMD, 0x01, 0xfc, 0x01, 0x5a };
	int err;

	bt_hci_pkt_reset_cmd(cmd);
	net_buf_simple_add_u8(cmd, 0x5a);

	err = bt_hci_lockstep_cmd_send_sync(&xport->ls, TEST_OPCODE, cmd, rsp);
	assert_int_equal(err, expected_err);

	assert_int_equal(xport->sent_len, sizeof(expected_pkt));
	assert_memory_equal(xport->sent, expected_pkt, sizeof(expected_pkt));
}

static void bt_hci_lockstep__test_cmd_complete_from_send(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);

	assert_int_equal(rsp.len, 3);
	assert_int_equal(rsp.data[0], BT_HCI_ERR_SUCCESS);
	assert_int_equal(rsp.data[1], 0xaa);
	assert_int_equal(rsp.data[2], 0xbb);
}

static void bt_hci_lockstep__test_cmd_complete_async(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	bt_work_schedule(&xport->rsp_work, BT_MSEC(20));

	send_test_cmd(xport, &cmd, &rsp, 0);

	assert_int_equal(rsp.len, 3);
	assert_int_equal(rsp.data[1], 0xaa);
}

static void bt_hci_lockstep__test_cmd_complete_error_status(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_err_rsp;
	xport->rsp_len = sizeof(cc_err_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, -EIO);

	assert_int_equal(rsp.len, 1);
	assert_int_equal(rsp.data[0], BT_HCI_ERR_UNSPECIFIED);
}

static void bt_hci_lockstep__test_cmd_status(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cs_ok_rsp;
	xport->rsp_len = sizeof(cs_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);

	/* A Command Status has nothing but its status */
	assert_int_equal(rsp.len, 1);
	assert_int_equal(rsp.data[0], BT_HCI_ERR_SUCCESS);
}

static void bt_hci_lockstep__test_no_rsp_buffer(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, NULL, 0);

	/* A following command with a response buffer is unaffected */
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(rsp.len, 3);
}

static void bt_hci_lockstep__test_rsp_truncated_to_buffer(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 2);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);

	assert_int_equal(rsp.len, 2);
	assert_int_equal(rsp.data[0], 0x00);
	assert_int_equal(rsp.data[1], 0xaa);
}

static void bt_hci_lockstep__test_timeout(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);

	send_test_cmd(xport, &cmd, &rsp, -EAGAIN);

	/* A late response is not consumed */
	assert_false(bt_hci_lockstep_feed(&xport->ls, cc_ok_rsp, sizeof(cc_ok_rsp)));
	assert_int_equal(rsp.len, 0);
}

static void bt_hci_lockstep__test_send_error(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->send_err = -EBUSY;

	send_test_cmd(xport, &cmd, &rsp, -EBUSY);

	/* The helper is not armed */
	assert_false(bt_hci_lockstep_feed(&xport->ls, cc_ok_rsp, sizeof(cc_ok_rsp)));
}

static void bt_hci_lockstep__test_feed_idle(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	fake_transport_init(xport);

	/* Nothing outstanding: nothing is consumed */
	assert_false(bt_hci_lockstep_feed(&xport->ls, cc_ok_rsp, sizeof(cc_ok_rsp)));
}

/* While a command is outstanding, packets that are not its response are
 * left untouched for the driver: a response to another opcode, an unrelated
 * event, the NOP Command Complete and a malformed event.
 */
static void check_not_consumed_while_waiting(const uint8_t *pre, size_t pre_len)
{
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->pre = pre;
	xport->pre_len = pre_len;
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);

	assert_false(xport->pre_consumed);
	assert_int_equal(rsp.len, 3);
}

static void bt_hci_lockstep__test_other_opcode_not_consumed(void **cm_state)
{
	(void)cm_state;
	check_not_consumed_while_waiting(cc_other_rsp, sizeof(cc_other_rsp));
}

static void bt_hci_lockstep__test_unrelated_event_not_consumed(void **cm_state)
{
	(void)cm_state;
	static const uint8_t le_meta[] = { BT_HCI_H4_EVT, BT_HCI_EVT_LE_META_EVENT, 0x01, 0x00 };

	check_not_consumed_while_waiting(le_meta, sizeof(le_meta));
}

static void bt_hci_lockstep__test_nop_cmd_complete_not_consumed(void **cm_state)
{
	(void)cm_state;
	check_not_consumed_while_waiting(nop_cc_grant, sizeof(nop_cc_grant));
}

static void bt_hci_lockstep__test_malformed_event_not_consumed(void **cm_state)
{
	(void)cm_state;
	static const uint8_t short_cc[] = { BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x02,
					    0x01, 0x01 };

	check_not_consumed_while_waiting(short_cc, sizeof(short_cc));
}



static void bt_hci_lockstep__test_cmd_status_error(void **cm_state)
{
	(void)cm_state;
	static const uint8_t cs_err_rsp[] = { BT_HCI_H4_EVT, BT_HCI_EVT_CMD_STATUS, 0x04,
					      BT_HCI_ERR_CMD_DISALLOWED, 0x01, 0x01, 0xfc };
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cs_err_rsp;
	xport->rsp_len = sizeof(cs_err_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, -EIO);

	assert_int_equal(rsp.len, 1);
	assert_int_equal(rsp.data[0], BT_HCI_ERR_CMD_DISALLOWED);
}

static void bt_hci_lockstep__test_cmd_complete_without_status(void **cm_state)
{
	(void)cm_state;
	/* Command Complete for the awaited opcode with no return parameters */
	static const uint8_t cc_no_status[] = { BT_HCI_H4_EVT, BT_HCI_EVT_CMD_COMPLETE, 0x03,
						0x01, 0x01, 0xfc };
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_no_status;
	xport->rsp_len = sizeof(cc_no_status);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, -EIO);

	/* No status byte arrived, so none is delivered */
	assert_int_equal(rsp.len, 0);
}

static void bt_hci_lockstep__test_too_many_params(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();
	int err;

	BT_HCI_PKT_CMD_DEFINE(cmd, UINT8_MAX + 1);

	fake_transport_init(xport);
	(void)net_buf_simple_add(&cmd, UINT8_MAX + 1);

	err = bt_hci_lockstep_cmd_send_sync(&xport->ls, TEST_OPCODE, &cmd, NULL);
	assert_int_equal(err, -EMSGSIZE);
	/* Nothing was sent and the helper is not armed */
	assert_int_equal(xport->sent_len, 0);
	assert_int_equal(cmd.len, UINT8_MAX + 1);
	assert_false(bt_hci_lockstep_feed(&xport->ls, cc_ok_rsp, sizeof(cc_ok_rsp)));
}

static void bt_hci_lockstep__test_cmd_without_params(void **cm_state)
{
	(void)cm_state;
	const uint8_t expected_pkt[] = { BT_HCI_H4_CMD, 0x01, 0xfc, 0x00 };
	struct fake_transport *xport = new_transport();
	int err;

	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	/* NULL stands for "no parameters": the helper frames the packet itself */
	err = bt_hci_lockstep_cmd_send_sync(&xport->ls, TEST_OPCODE, NULL, &rsp);
	assert_int_equal(err, 0);
	assert_int_equal(xport->sent_len, sizeof(expected_pkt));
	assert_memory_equal(xport->sent, expected_pkt, sizeof(expected_pkt));
	assert_int_equal(rsp.len, 3);
}

static void bt_hci_lockstep__test_reuse(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(rsp.len, 3);

	xport->rsp = cs_ok_rsp;
	xport->rsp_len = sizeof(cs_ok_rsp);

	/* The response buffer is emptied for the new command, which gets a
	 * Command Status: its status alone.
	 */
	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(rsp.len, 1);
	assert_int_equal(rsp.data[0], BT_HCI_ERR_SUCCESS);
}

/* Command flow control: a response that allows no further command holds the
 * next one until the controller allows it, here with the NOP Command Complete
 * it sends for that purpose, which is not consumed.
 */
static void check_no_credit_holds_next_send(const uint8_t *rsp_pkt, size_t rsp_len)
{
	struct fake_transport *xport = new_transport();
	int err;

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = rsp_pkt;
	xport->rsp_len = rsp_len;
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 1);

	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);

	bt_hci_pkt_reset_cmd(&cmd);
	net_buf_simple_add_u8(&cmd, 0x5a);
	err = bt_hci_lockstep_cmd_send_sync(&xport->ls, TEST_OPCODE, &cmd, &rsp);
	assert_int_equal(err, -EAGAIN);
	assert_int_equal(xport->sends, 1);

	assert_false(bt_hci_lockstep_feed(&xport->ls, nop_cc_grant, sizeof(nop_cc_grant)));

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 2);
	assert_int_equal(rsp.len, 3);
}

static void bt_hci_lockstep__test_cmd_complete_no_credit(void **cm_state)
{
	(void)cm_state;
	check_no_credit_holds_next_send(cc_ok_no_credit_rsp, sizeof(cc_ok_no_credit_rsp));
}

static void bt_hci_lockstep__test_cmd_status_no_credit(void **cm_state)
{
	(void)cm_state;
	check_no_credit_holds_next_send(cs_ok_no_credit_rsp, sizeof(cs_ok_no_credit_rsp));
}

static void bt_hci_lockstep__test_send_error_keeps_credit(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->send_err = -EBUSY;

	send_test_cmd(xport, &cmd, &rsp, -EBUSY);

	/* Nothing reached the controller, so the next command is allowed */
	xport->send_err = 0;
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(rsp.len, 3);
}

static void bt_hci_lockstep__test_timeout_uses_credit(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();
	int err;

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);

	send_test_cmd(xport, &cmd, &rsp, -EAGAIN);
	assert_int_equal(xport->sends, 1);

	/* The unanswered command used up the allowance */
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	bt_hci_pkt_reset_cmd(&cmd);
	net_buf_simple_add_u8(&cmd, 0x5a);
	err = bt_hci_lockstep_cmd_send_sync(&xport->ls, TEST_OPCODE, &cmd, &rsp);
	assert_int_equal(err, -EAGAIN);
	assert_int_equal(xport->sends, 1);

	/* Until the controller allows a command again */
	assert_false(bt_hci_lockstep_feed(&xport->ls, nop_cc_grant, sizeof(nop_cc_grant)));

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 2);
}

/* A controller reset by other means than an HCI command allows a command
 * again without announcing it, which is what bt_hci_lockstep_reset() is for.
 */
static void bt_hci_lockstep__test_reset_restores_the_allowance(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);

	send_test_cmd(xport, &cmd, &rsp, -EAGAIN);
	assert_int_equal(xport->sends, 1);

	bt_hci_lockstep_reset(&xport->ls);

	/* The send function and the timeout survive the reset */
	assert_true(BT_TIMEOUT_EQ(xport->ls.timeout, BT_MSEC(100)));

	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 2);
	assert_int_equal(rsp.len, 3);
}

/* A driver resets the helper every time it reopens its transport, so the
 * operation has to survive being repeated.
 */
static void bt_hci_lockstep__test_reset_is_repeatable(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);

	bt_hci_lockstep_reset(&xport->ls);
	bt_hci_lockstep_reset(&xport->ls);

	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(rsp.len, 3);
}

/* A reset abandons the outstanding command: its response is no longer
 * consumed, and its sender fails when its timeout has run out and not earlier,
 * so that the response cannot be taken for that of the next command. The
 * helper is ready for that one afterwards.
 */
static void bt_hci_lockstep__test_reset_abandons_the_command_in_progress(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();
	int64_t start;

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);

	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;
	xport->reset_in_send = true;

	start = bt_uptime_get_ms();
	send_test_cmd(xport, &cmd, &rsp, -EAGAIN);
	assert_true(bt_uptime_get_ms() - start >= 100);
	assert_int_equal(xport->sends, 1);
	assert_int_equal(rsp.len, 0);

	xport->reset_in_send = false;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 2);
	assert_int_equal(rsp.len, 3);
}

/* A send that is waiting for the controller to allow its command goes ahead
 * when a reset restores the allowance, as when a recovery path resets a
 * controller that has stopped responding.
 */
static void bt_hci_lockstep__test_reset_releases_a_send_waiting_for_the_allowance(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);

	/* Leaves the helper without an allowance */
	xport->rsp = cc_ok_no_credit_rsp;
	xport->rsp_len = sizeof(cc_ok_no_credit_rsp);
	xport->rsp_in_send = true;

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 1);

	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->reset_from_work = true;
	bt_work_schedule(&xport->rsp_work, BT_MSEC(20));

	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 2);
	assert_int_equal(rsp.len, 3);
}





static void bt_hci_lockstep__test_nop_while_waiting_is_no_response(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = nop_cc_grant;
	xport->rsp_len = sizeof(nop_cc_grant);
	xport->rsp_in_send = true;

	/* The NOP wakes the sender, which keeps waiting for its response */
	send_test_cmd(xport, &cmd, &rsp, -EAGAIN);
	assert_int_equal(rsp.len, 0);
}

/* A transport failure reported after the controller's allowance changed
 * leaves that allowance as the controller set it.
 */
static void check_send_error_keeps_allowance(const uint8_t *nop, size_t nop_len,
					     int second_err, unsigned int sends)
{
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = nop;
	xport->rsp_len = nop_len;
	xport->rsp_in_send = true;
	xport->send_err = -EIO;

	send_test_cmd(xport, &cmd, &rsp, -EIO);
	send_test_cmd(xport, &cmd, &rsp, second_err);
	assert_int_equal(xport->sends, sends);
}

static void bt_hci_lockstep__test_send_error_after_revoke(void **cm_state)
{
	(void)cm_state;
	check_send_error_keeps_allowance(nop_cc_revoke, sizeof(nop_cc_revoke), -EAGAIN, 1);
}

static void bt_hci_lockstep__test_send_error_after_grant(void **cm_state)
{
	(void)cm_state;
	check_send_error_keeps_allowance(nop_cc_grant, sizeof(nop_cc_grant), -EIO, 2);
}

static void bt_hci_lockstep__test_response_before_send_error(void **cm_state)
{
	(void)cm_state;
	struct fake_transport *xport = new_transport();

	BT_HCI_PKT_CMD_DEFINE(cmd, 1);
	NET_BUF_SIMPLE_DEFINE(rsp, 8);

	fake_transport_init(xport);
	xport->rsp = cc_ok_rsp;
	xport->rsp_len = sizeof(cc_ok_rsp);
	xport->rsp_in_send = true;
	xport->send_err = -EIO;

	/* The response arrived, so the command reached the controller and
	 * the transport's failure does not count.
	 */
	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(rsp.len, 3);
	assert_int_equal(rsp.data[0], BT_HCI_ERR_SUCCESS);

	/* And its allowance was replaced by the response's */
	xport->send_err = 0;
	send_test_cmd(xport, &cmd, &rsp, 0);
	assert_int_equal(xport->sends, 2);
}





static int bt_test_bt_hci_lockstep_after(void **state)
{
	(void)state;
	while (owned_count) {
		struct fake_transport *xport = owned_transports[--owned_count];

		clear_transport(xport);
		free(xport);
	}
	return 0;
}

static const struct CMUnitTest test_cases_bt_hci_lockstep[] = {
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_complete_from_send, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_complete_async, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_complete_error_status, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_status, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_no_rsp_buffer, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_rsp_truncated_to_buffer, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_timeout, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_send_error, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_feed_idle, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_other_opcode_not_consumed, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_unrelated_event_not_consumed, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_nop_cmd_complete_not_consumed, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_malformed_event_not_consumed, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_status_error, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_complete_without_status, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_too_many_params, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_without_params, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_reuse, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_complete_no_credit, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_cmd_status_no_credit, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_send_error_keeps_credit, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_timeout_uses_credit, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_reset_restores_the_allowance, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_reset_is_repeatable, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_reset_abandons_the_command_in_progress, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_reset_releases_a_send_waiting_for_the_allowance, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_nop_while_waiting_is_no_response, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_send_error_after_revoke, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_send_error_after_grant, NULL, bt_test_bt_hci_lockstep_after),
	cmocka_unit_test_setup_teardown(bt_hci_lockstep__test_response_before_send_error, NULL, bt_test_bt_hci_lockstep_after),
};
BT_TEST_SUITE(bt_hci_lockstep, test_cases_bt_hci_lockstep, bt_test_base_setup, NULL);
