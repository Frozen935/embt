#include "support/test_support.h"
static const struct bt_hci_transport test_driver;
#include "host/hci_core.h"
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/l2cap.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/base/atomic.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include "host/conn_internal.h"
#include "host/l2cap_internal.h"


#define GENERIC_RP_SIZE 64U
#define LE_ACL_MAX_LEN  27U
#define HANDLE          0x0001U

/* Headers of an ACL packet from the controller, after its H:4 packet type */
#define ACL_HDR_OFFSET   1U
#define L2CAP_HDR_OFFSET (ACL_HDR_OFFSET + sizeof(struct bt_hci_acl_hdr))
#define SIG_HDR_OFFSET   (L2CAP_HDR_OFFSET + sizeof(struct bt_l2cap_hdr))

#define DATA        "data"
#define PSM         0x0080U
#define UNKNOWN_PSM 0x0081U
#define PEER_CID    0x0040U
#define PEER_IDENT  0x80U
#define LE_MIN_MTU  23U

/* How long to wait for a PDU that the Host is to send. It sends it without
 * simulated time passing, so this only bounds a failed wait.
 */
#define TX_TIMEOUT BT_MSEC(100)

/* One SDU is sent */
NET_BUF_POOL_FIXED_DEFINE(sdu_pool, 1, BT_L2CAP_SDU_CHAN_SEND_RESERVE + sizeof(DATA),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);
/* The PDUs that the Host sent and the test has not taken yet: at most a signalling
 * response and the SDU, which are all that it has to send at once
 */
NET_BUF_POOL_FIXED_DEFINE(sent_pool, 2, LE_ACL_MAX_LEN, 0, NULL);

static struct bt_conn *conn;
static struct bt_l2cap_le_chan chan;

/* The L2CAP PDUs that the Host sent */
static BT_FIFO_DEFINE(sent);

/* The controller has one ACL buffer, which it reports free again unless the
 * test holds it, as for a peer that does not acknowledge at the link layer.
 */
static bt_atomic_t unreported;
static bool hold;

static void evt_create(struct net_buf *buf, uint8_t evt, uint8_t len)
{
	struct bt_hci_evt_hdr *hdr;

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = evt;
	hdr->len = len;
}

static void *cmd_complete(struct net_buf **buf, uint8_t plen, uint16_t opcode)
{
	struct bt_hci_evt_cmd_complete *cc;

	*buf = bt_buf_get_evt(BT_HCI_EVT_CMD_COMPLETE, false, BT_FOREVER);
	evt_create(*buf, BT_HCI_EVT_CMD_COMPLETE, sizeof(*cc) + plen);
	cc = net_buf_add(*buf, sizeof(*cc));
	cc->ncmd = 1U;
	cc->opcode = sys_cpu_to_le16(opcode);

	return net_buf_add(*buf, plen);
}

static void generic_success(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_evt_cc_status *rp;

	rp = cmd_complete(evt, GENERIC_RP_SIZE, opcode);
	(void)memset(rp, 0, GENERIC_RP_SIZE);
	rp->status = BT_HCI_ERR_SUCCESS;
}

static void read_local_features(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_read_local_features *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->features, 0, sizeof(rp->features));
	/* LE supported, BR/EDR not supported */
	rp->features[4] = BIT(5) | BIT(6);
}

static void read_supported_commands(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_read_supported_commands *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->commands, 0xFF, sizeof(rp->commands));
}

static void read_bd_addr(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_read_bd_addr *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->bdaddr = (bt_addr_t){ .val = { 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6 } };
}

static void le_read_buffer_size(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_le_read_buffer_size *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->le_max_len = sys_cpu_to_le16(LE_ACL_MAX_LEN);
	rp->le_max_num = 1U;
}

static void le_read_supp_states(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_le_read_supp_states *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->le_states, 0xFF, sizeof(rp->le_states));
}

static void cmd_handle(const struct bt_hci_transport *dev, struct net_buf *cmd)
{
	struct net_buf *evt = NULL;
	struct bt_hci_cmd_hdr *hdr;
	uint16_t opcode;

	hdr = net_buf_pull_mem(cmd, sizeof(*hdr));
	opcode = sys_le16_to_cpu(hdr->opcode);

	switch (opcode) {
	case BT_HCI_OP_HOST_NUM_COMPLETED_PACKETS:
		/* Has no response */
		return;
	case BT_HCI_OP_READ_LOCAL_FEATURES:
		read_local_features(&evt, opcode);
		break;
	case BT_HCI_OP_READ_SUPPORTED_COMMANDS:
		read_supported_commands(&evt, opcode);
		break;
	case BT_HCI_OP_READ_BD_ADDR:
		read_bd_addr(&evt, opcode);
		break;
	case BT_HCI_OP_LE_READ_BUFFER_SIZE:
		le_read_buffer_size(&evt, opcode);
		break;
	case BT_HCI_OP_LE_READ_SUPP_STATES:
		le_read_supp_states(&evt, opcode);
		break;
	default:
		generic_success(&evt, opcode);
		break;
	}

	bt_hci_recv(dev, evt);
}

static void completed_handler(struct bt_work *work)
{
	const struct bt_hci_transport *dev = &test_driver;
	struct bt_hci_evt_num_completed_packets *ev;
	struct bt_hci_handle_count *hc;
	struct net_buf *evt;
	bt_atomic_val_t count;

	ARG_UNUSED(work);

	count = bt_atomic_clear(&unreported);
	if (count == 0) {
		return;
	}

	evt = bt_buf_get_evt(BT_HCI_EVT_NUM_COMPLETED_PACKETS, false, BT_FOREVER);
	evt_create(evt, BT_HCI_EVT_NUM_COMPLETED_PACKETS, sizeof(*ev) + sizeof(*hc));
	ev = net_buf_add(evt, sizeof(*ev));
	ev->num_handles = 1U;
	hc = net_buf_add(evt, sizeof(*hc));
	hc->handle = sys_cpu_to_le16(HANDLE);
	hc->count = sys_cpu_to_le16((uint16_t)count);

	bt_hci_recv(dev, evt);
}

static BT_WORK_DEFINE(completed_work, completed_handler);

static void release(void)
{
	hold = false;
	assert_true(bt_work_submit(&completed_work) >= 0);
}

static void acl_handle(struct net_buf *acl)
{
	struct bt_hci_acl_hdr *hdr;
	struct net_buf *pdu;

	/* Every PDU fits a single ACL packet */
	hdr = net_buf_pull_mem(acl, sizeof(*hdr));
	assert_int_equal(bt_acl_handle(sys_le16_to_cpu(hdr->handle)), HANDLE);

	pdu = net_buf_alloc(&sent_pool, BT_NO_WAIT);
	assert_non_null(pdu);
	(void)net_buf_add_mem(pdu, acl->data, acl->len);
	bt_fifo_put(&sent, pdu);

	(void)bt_atomic_inc(&unreported);
	if (!hold) {
		assert_true(bt_work_submit(&completed_work) >= 0);
	}
}

static int driver_open(const struct bt_hci_transport *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int driver_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	uint8_t type = net_buf_pull_u8(buf);

	if (type == BT_HCI_H4_CMD) {
		cmd_handle(dev, buf);
	} else if (type == BT_HCI_H4_ACL) {
		acl_handle(buf);
	}

	net_buf_unref(buf);

	return 0;
}

static const struct bt_hci_driver_api driver_api = {
	.open = driver_open,
	.send = driver_send,
};

static struct bt_hci_driver_data test_driver_data;
static const struct bt_hci_transport test_driver = {
 .name = "upstream-test", .bus = 0,
 .api = &driver_api, .data = &test_driver_data,
};
static void test_transport_init(void)
{
 bt_test_require(bt_stack_init());
 bt_dev.hci = &test_driver;
}


/* The next L2CAP PDU that the Host sent, with its header pulled */
static struct net_buf *tx(uint16_t cid)
{
	struct bt_l2cap_hdr *hdr;
	struct net_buf *buf;

	buf = bt_fifo_get(&sent, TX_TIMEOUT);
	assert_non_null(buf);

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	assert_int_equal(sys_le16_to_cpu(hdr->cid), cid);

	return buf;
}

/* The next PDU must be a signalling command with the given code. Returns its
 * identifier.
 */
static uint8_t tx_sig(uint8_t code)
{
	struct bt_l2cap_sig_hdr *sig;
	struct net_buf *buf;
	uint8_t ident;

	buf = tx(BT_L2CAP_CID_LE_SIG);

	sig = net_buf_pull_mem(buf, sizeof(*sig));
	assert_int_equal(sig->code, code);
	ident = sig->ident;

	net_buf_unref(buf);

	return ident;
}

static struct net_buf *sig_cmd(uint8_t code, uint8_t ident)
{
	struct bt_l2cap_sig_hdr *sig;
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_FOREVER);
	(void)net_buf_add(buf, sizeof(struct bt_hci_acl_hdr) + sizeof(struct bt_l2cap_hdr));

	sig = net_buf_add(buf, sizeof(*sig));
	sig->code = code;
	sig->ident = ident;

	return buf;
}

/* Deliver the command and let the Host handle it, which takes no simulated time */
static void recv_sig(struct net_buf *buf)
{
	struct bt_hci_acl_hdr *acl = (void *)&buf->data[ACL_HDR_OFFSET];
	struct bt_l2cap_hdr *hdr = (void *)&buf->data[L2CAP_HDR_OFFSET];
	struct bt_l2cap_sig_hdr *sig = (void *)&buf->data[SIG_HDR_OFFSET];

	sig->len = sys_cpu_to_le16(buf->len - SIG_HDR_OFFSET - sizeof(*sig));
	hdr->len = sys_cpu_to_le16(buf->len - SIG_HDR_OFFSET);
	hdr->cid = sys_cpu_to_le16(BT_L2CAP_CID_LE_SIG);
	acl->handle = sys_cpu_to_le16(bt_acl_handle_pack(HANDLE, BT_ACL_START));
	acl->len = sys_cpu_to_le16(buf->len - L2CAP_HDR_OFFSET);

	bt_hci_recv(&test_driver, buf);
	(void)osi_sleep_ms(BT_MSEC(1));
}

/* A connection request for a PSM without a server, which the Host refuses */
static void recv_unknown_conn_req(uint8_t ident)
{
	struct net_buf *buf;

	buf = sig_cmd(BT_L2CAP_LE_CONN_REQ, ident);
	net_buf_add_le16(buf, UNKNOWN_PSM);
	net_buf_add_le16(buf, PEER_CID + 1U);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, 1U);
	recv_sig(buf);
}

static int chan_recv(struct bt_l2cap_chan *ch, struct net_buf *buf)
{
	return 0;
}

static const struct bt_l2cap_chan_ops chan_ops = {
	.recv = chan_recv,
};

/* Connect a credit-based channel, to which the peer gives no credits */
static void *setup(void)
{
	static const bt_addr_le_t addr = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = { .val = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } },
	};
	struct net_buf *buf;
	uint8_t ident;

	assert_int_equal(bt_enable(NULL), 0);

	/* As on Connection Complete */
	conn = bt_conn_add_le(BT_ID_DEFAULT, &addr);
	assert_non_null(conn);
	conn->handle = HANDLE;
	bt_conn_set_state(conn, BT_CONN_CONNECTED);
	bt_l2cap_connected(conn);

	chan.chan.ops = &chan_ops;
	chan.rx.mtu = LE_MIN_MTU;
	assert_int_equal(bt_l2cap_chan_connect(conn, &chan.chan, PSM), 0);

	ident = tx_sig(BT_L2CAP_LE_CONN_REQ);
	buf = sig_cmd(BT_L2CAP_LE_CONN_RSP, ident);
	net_buf_add_le16(buf, PEER_CID);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, 0U);
	net_buf_add_le16(buf, BT_L2CAP_LE_SUCCESS);
	recv_sig(buf);

	assert_int_equal(chan.state, BT_L2CAP_CONNECTED);
	assert_int_equal(bt_atomic_get(&chan.tx.credits), 0);

	return NULL;
}



/* A channel without credits does not hold up another channel of the connection */
static void host_l2cap_le_tx__test_no_credits(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf;

	/* A response takes the controller's buffer until release() */
	hold = true;
	recv_unknown_conn_req(PEER_IDENT);
	assert_int_equal(tx_sig(BT_L2CAP_LE_CONN_RSP), PEER_IDENT);

	/* Queued meanwhile: an SDU on the channel without credits, and behind it
	 * a response on the signalling channel
	 */
	buf = net_buf_alloc(&sdu_pool, BT_NO_WAIT);
	assert_non_null(buf);
	net_buf_reserve(buf, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
	(void)net_buf_add_mem(buf, DATA, strlen(DATA));
	assert_int_equal(bt_l2cap_chan_send(&chan.chan, buf), 0);

	recv_unknown_conn_req(PEER_IDENT + 1U);

	release();

	assert_int_equal(tx_sig(BT_L2CAP_LE_CONN_RSP), PEER_IDENT + 1U);

	/* The SDU follows its credit */
	buf = sig_cmd(BT_L2CAP_LE_CREDITS, PEER_IDENT + 2U);
	net_buf_add_le16(buf, PEER_CID);
	net_buf_add_le16(buf, 1U);
	recv_sig(buf);

	/* A single K-frame: the SDU length, then the SDU */
	buf = tx(PEER_CID);
	assert_int_equal(net_buf_pull_le16(buf), strlen(DATA));
	assert_int_equal(buf->len, strlen(DATA));
	assert_memory_equal(buf->data, DATA, strlen(DATA));
	net_buf_unref(buf);
}

static void storage_init_7767618e85(void)
{
	net_buf_pool_init(&sdu_pool);
	net_buf_pool_init(&sent_pool);
	bt_fifo_init(&sent);
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&sdu_pool);
	net_buf_pool_init(&sent_pool);
}

int bt_test_host_l2cap_le_tx_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	bt_test_require(bt_conn_init());
	migrated_pools_init();
	test_transport_init();
	storage_init_7767618e85();
	*state = setup();
	return 0;
}



static const struct CMUnitTest test_cases_host_l2cap_le_tx[] = {
	cmocka_unit_test(host_l2cap_le_tx__test_no_credits),
};
BT_TEST_SUITE(host_l2cap_le_tx, test_cases_host_l2cap_le_tx, bt_test_host_l2cap_le_tx_setup, NULL);
