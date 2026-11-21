#include "support/test_support.h"
#include "host/conn_internal.h"
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/conn.h>

#include <embt/net_buf.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include "host/classic/conn_br_internal.h"

#define L2CAP_HDR_LEN   4U
#define MAX_PAYLOAD_LEN 8U
/* One more PDU than bt_br_acl_recv() has views for, plus the last one */
#define MAX_PDUS        (CONFIG_BT_BR_ACL_RX_VIEW_COUNT + 2)
#define KEEP_ALL        SIZE_MAX

/* bt_br_acl_recv() only passes the connection on to bt_l2cap_recv() */
static uint8_t conn_storage;
#define TEST_CONN ((struct bt_conn *)&conn_storage)

struct rx_pdu {
	struct net_buf *buf;
	uint8_t *data;
	uint16_t len;
	uint8_t bytes[L2CAP_HDR_LEN + MAX_PAYLOAD_LEN];
};

static struct rx_pdu rx[MAX_PDUS];
static size_t rx_count;
/* How many of the first PDUs the receiver keeps, as an RFCOMM recv() returning
 * -EINPROGRESS does; the others are consumed during bt_l2cap_recv().
 */
static size_t rx_keep;
static size_t acl_freed;

void __wrap_bt_l2cap_recv(struct bt_conn *conn, struct net_buf *buf, bool complete)
{
	struct rx_pdu *pdu;

	assert_ptr_equal(conn, TEST_CONN);
	assert_true(complete);
	assert_true(rx_count < ARRAY_SIZE(rx));
	assert_true(buf->len <= sizeof(pdu->bytes));

	pdu = &rx[rx_count];
	pdu->buf = buf;
	pdu->data = buf->data;
	pdu->len = buf->len;
	memcpy(pdu->bytes, buf->data, buf->len);

	if (rx_count >= rx_keep) {
		net_buf_unref(buf);
	}

	rx_count++;
}

static void rx_freed(enum bt_buf_type type)
{
	if ((type & BT_BUF_ACL_IN) != 0) {
		acl_freed++;
	}
}

static uint8_t payload_byte(size_t pdu, size_t i)
{
	return (uint8_t)((pdu << 4) + i);
}

/* One HCI ACL packet payload holding an L2CAP PDU per entry of payload_lens */
static struct net_buf *acl_packet(const uint16_t *payload_lens, size_t count)
{
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_NO_WAIT);
	assert_non_null(buf);

	/* Drop the H4 packet type, as the HCI core does before L2CAP sees the buffer */
	(void)net_buf_pull_u8(buf);

	for (size_t pdu = 0; pdu < count; pdu++) {
		assert_true(payload_lens[pdu] <= MAX_PAYLOAD_LEN);
		assert_true(net_buf_tailroom(buf) >= L2CAP_HDR_LEN + payload_lens[pdu]);

		net_buf_add_le16(buf, payload_lens[pdu]);
		net_buf_add_le16(buf, 0x0040U + pdu);

		for (size_t i = 0; i < payload_lens[pdu]; i++) {
			net_buf_add_u8(buf, payload_byte(pdu, i));
		}
	}

	return buf;
}

static void check_pdu(size_t pdu, const uint8_t *data, uint16_t len, uint16_t payload_len)
{
	assert_int_equal(len, L2CAP_HDR_LEN + payload_len);
	assert_int_equal(sys_get_le16(&data[0]), payload_len);
	assert_int_equal(sys_get_le16(&data[2]), 0x0040U + pdu);

	for (size_t i = 0; i < payload_len; i++) {
		assert_int_equal(data[L2CAP_HDR_LEN + i], payload_byte(pdu, i));
	}
}

/* The PDU as bt_l2cap_recv() got it */
static void check_delivered(size_t pdu, uint16_t payload_len)
{
	check_pdu(pdu, rx[pdu].bytes, rx[pdu].len, payload_len);
}

/* The buffer a kept PDU arrived in still shows that PDU */
static void check_kept(size_t pdu, uint16_t payload_len)
{
	struct net_buf *buf = rx[pdu].buf;

	assert_ptr_equal(buf->data, rx[pdu].data);
	check_pdu(pdu, buf->data, buf->len, payload_len);
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	memset(rx, 0, sizeof(rx));
	rx_count = 0;
	rx_keep = 0;
	acl_freed = 0;
	bt_buf_rx_freed_cb_set(rx_freed);
}



/* A receiver that keeps the only PDU of a packet sees it after bt_br_acl_recv() returns */
static void br_acl_recv__test_single_pdu_kept(void **cm_state)
{
	(void)cm_state;
	static const uint16_t lens[] = { 5 };
	struct net_buf *acl = acl_packet(lens, ARRAY_SIZE(lens));

	rx_keep = KEEP_ALL;
	bt_br_acl_recv(TEST_CONN, acl, true);

	assert_int_equal(rx_count, 1);
	assert_ptr_equal(rx[0].buf, acl);
	check_kept(0, lens[0]);
	assert_int_equal(acl_freed, 0);

	net_buf_unref(rx[0].buf);
	assert_int_equal(acl_freed, 1);
}

/* Each PDU of a packet carrying several keeps its own buffer, and the ACL
 * buffer is freed exactly once, after the last of them is released.
 */
static void br_acl_recv__test_multiple_pdus_kept(void **cm_state)
{
	(void)cm_state;
	static const uint16_t lens[] = { 3, 1, 7 };
	struct net_buf *acl = acl_packet(lens, ARRAY_SIZE(lens));

	rx_keep = KEEP_ALL;
	bt_br_acl_recv(TEST_CONN, acl, true);

	assert_int_equal(rx_count, ARRAY_SIZE(lens));
	assert_ptr_equal(rx[ARRAY_SIZE(lens) - 1].buf, acl);
	assert_int_not_equal(rx[0].buf, rx[1].buf);

	for (size_t pdu = 0; pdu < ARRAY_SIZE(lens); pdu++) {
		check_kept(pdu, lens[pdu]);
	}

	/* A push on a view cannot reach the PDUs before it */
	for (size_t pdu = 0; pdu < ARRAY_SIZE(lens) - 1; pdu++) {
		assert_int_equal(net_buf_headroom(rx[pdu].buf), 0U);
	}

	net_buf_unref(rx[2].buf);
	net_buf_unref(rx[0].buf);
	assert_int_equal(acl_freed, 0);

	net_buf_unref(rx[1].buf);
	assert_int_equal(acl_freed, 1);
}

/* A receiver that consumes each PDU synchronously sees every PDU once */
static void br_acl_recv__test_multiple_pdus_released(void **cm_state)
{
	(void)cm_state;
	static const uint16_t lens[] = { 2, 4, 6 };
	struct net_buf *acl = acl_packet(lens, ARRAY_SIZE(lens));

	bt_br_acl_recv(TEST_CONN, acl, true);

	assert_int_equal(rx_count, ARRAY_SIZE(lens));

	for (size_t pdu = 0; pdu < ARRAY_SIZE(lens); pdu++) {
		check_delivered(pdu, lens[pdu]);
	}

	assert_int_equal(acl_freed, 1);
}

/* With every view kept by earlier receivers, later PDUs are still delivered */
static void br_acl_recv__test_views_exhausted(void **cm_state)
{
	(void)cm_state;
	uint16_t lens[MAX_PDUS];
	struct net_buf *acl;
	size_t views = MAX_PDUS - 2;

	for (size_t pdu = 0; pdu < ARRAY_SIZE(lens); pdu++) {
		lens[pdu] = 1;
	}

	acl = acl_packet(lens, ARRAY_SIZE(lens));

	rx_keep = views;
	bt_br_acl_recv(TEST_CONN, acl, true);

	assert_int_equal(rx_count, ARRAY_SIZE(lens));

	for (size_t pdu = 0; pdu < ARRAY_SIZE(lens); pdu++) {
		check_delivered(pdu, lens[pdu]);
	}

	for (size_t pdu = 0; pdu < views; pdu++) {
		assert_int_not_equal(rx[pdu].buf, acl);
		check_kept(pdu, lens[pdu]);
	}

	assert_ptr_equal(rx[views].buf, acl);

	assert_int_equal(acl_freed, 0);

	for (size_t pdu = 0; pdu < views; pdu++) {
		net_buf_unref(rx[pdu].buf);
	}

	assert_int_equal(acl_freed, 1);
}

/* Bytes after the last PDU that are too few for an L2CAP header are dropped */
static void br_acl_recv__test_trailing_bytes(void **cm_state)
{
	(void)cm_state;
	static const uint16_t lens[] = { 3 };

	for (size_t trailing = 1; trailing < L2CAP_HDR_LEN; trailing++) {
		struct net_buf *acl = acl_packet(lens, ARRAY_SIZE(lens));

		for (size_t i = 0; i < trailing; i++) {
			net_buf_add_u8(acl, 0xff);
		}

		rx_count = 0;
		acl_freed = 0;
		bt_br_acl_recv(TEST_CONN, acl, true);

		assert_int_equal(rx_count, 1);
		check_delivered(0, lens[0]);
		assert_int_equal(acl_freed, 1);
	}
}

int bt_test_br_acl_recv_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	bt_test_select_unopened_hci();
	bt_test_require(bt_conn_init());
	*state = NULL;
	return 0;
}
static int bt_test_br_acl_recv_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}


static const struct CMUnitTest test_cases_br_acl_recv[] = {
	cmocka_unit_test_setup_teardown(br_acl_recv__test_single_pdu_kept, bt_test_br_acl_recv_before, NULL),
	cmocka_unit_test_setup_teardown(br_acl_recv__test_multiple_pdus_kept, bt_test_br_acl_recv_before, NULL),
	cmocka_unit_test_setup_teardown(br_acl_recv__test_multiple_pdus_released, bt_test_br_acl_recv_before, NULL),
	cmocka_unit_test_setup_teardown(br_acl_recv__test_views_exhausted, bt_test_br_acl_recv_before, NULL),
	cmocka_unit_test_setup_teardown(br_acl_recv__test_trailing_bytes, bt_test_br_acl_recv_before, NULL),
};
BT_TEST_SUITE(br_acl_recv, test_cases_br_acl_recv, bt_test_br_acl_recv_setup, NULL);
