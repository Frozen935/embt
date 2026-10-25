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

/* One SDU is sent at a time */
NET_BUF_POOL_FIXED_DEFINE(sdu_pool, 1, BT_L2CAP_SDU_CHAN_SEND_RESERVE + sizeof(DATA),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);
/* The PDUs that the Host sent and the test has not taken yet: at most a signalling
 * response and the SDU, which are all that it has to send at once
 */
NET_BUF_POOL_FIXED_DEFINE(sent_pool, 2, LE_ACL_MAX_LEN, 0, NULL);

static struct bt_conn *conn;
static struct bt_l2cap_le_chan chan;
static os_sem_t connected_sem;
static os_sem_t released_sem;
static os_sem_t shutdown_sem;
static void chan_disconnected(struct bt_l2cap_chan *ch);

/* The L2CAP PDUs that the Host sent */
static BT_FIFO_DEFINE(sent);

/* The controller has one ACL buffer, which it reports free again unless the
 * test holds it, as for a peer that does not acknowledge at the link layer.
 */
static bt_atomic_t unreported;
static bool hold;
/* An asynchronous transport may retain the first disconnect PDU until the test releases it. */
static bool keep_disconnect;
static struct net_buf *kept_disconnect;

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
		const size_t sig_offset = sizeof(struct bt_hci_acl_hdr) + sizeof(struct bt_l2cap_hdr);
		bool retain = false;

		if (keep_disconnect && buf->len >= sig_offset + sizeof(struct bt_l2cap_sig_hdr) &&
		    sys_get_le16(buf->data + sizeof(struct bt_hci_acl_hdr) + sizeof(uint16_t)) ==
			BT_L2CAP_CID_LE_SIG && buf->data[sig_offset] == BT_L2CAP_DISCONN_REQ) {
			kept_disconnect = buf;
			retain = true;
		}
		acl_handle(buf);
		if (retain) {
			return 0;
		}
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

/* Deliver the PDU, whose payload starts at SIG_HDR_OFFSET, and let the Host
 * handle it, which takes no simulated time
 */
static void recv_pdu(struct net_buf *buf, uint16_t cid)
{
	struct bt_hci_acl_hdr *acl = (void *)&buf->data[ACL_HDR_OFFSET];
	struct bt_l2cap_hdr *hdr = (void *)&buf->data[L2CAP_HDR_OFFSET];

	hdr->len = sys_cpu_to_le16(buf->len - SIG_HDR_OFFSET);
	hdr->cid = sys_cpu_to_le16(cid);
	acl->handle = sys_cpu_to_le16(bt_acl_handle_pack(HANDLE, BT_ACL_START));
	acl->len = sys_cpu_to_le16(buf->len - L2CAP_HDR_OFFSET);

	bt_hci_recv(&test_driver, buf);
}

static void recv_sig(struct net_buf *buf)
{
	struct bt_l2cap_sig_hdr *sig = (void *)&buf->data[SIG_HDR_OFFSET];

	sig->len = sys_cpu_to_le16(buf->len - SIG_HDR_OFFSET - sizeof(*sig));
	recv_pdu(buf, BT_L2CAP_CID_LE_SIG);
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

static void chan_connected(struct bt_l2cap_chan *ch)
{
	bt_test_require(osi_sem_give(&connected_sem));
}

static const struct bt_l2cap_chan_ops chan_ops = {
	.connected = chan_connected,
	.released = chan_disconnected,
	.recv = chan_recv,
};

/* Connect a credit-based channel, to which the peer gives no credits */
static void chan_connect(struct bt_l2cap_le_chan *ch, const struct bt_l2cap_chan_ops *ops,
			 uint16_t peer_cid)
{
	struct net_buf *buf;
	uint8_t ident;

	ch->chan.ops = ops;
	ch->rx.mtu = LE_MIN_MTU;
	assert_int_equal(bt_l2cap_chan_connect(conn, &ch->chan, PSM), 0);

	ident = tx_sig(BT_L2CAP_LE_CONN_REQ);
	buf = sig_cmd(BT_L2CAP_LE_CONN_RSP, ident);
	net_buf_add_le16(buf, peer_cid);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, 0U);
	net_buf_add_le16(buf, BT_L2CAP_LE_SUCCESS);
	recv_sig(buf);
	assert_int_equal(osi_sem_take(&connected_sem, TX_TIMEOUT), 0);

	assert_int_equal(ch->state, BT_L2CAP_CONNECTED);
	assert_int_equal(bt_atomic_get(&ch->tx.credits), 0);
}

static void *setup(void)
{
	static const bt_addr_le_t addr = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = { .val = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } },
	};

	assert_int_equal(bt_enable(NULL), 0);

	/* As on Connection Complete */
	conn = bt_conn_add_le(BT_ID_DEFAULT, &addr);
	assert_non_null(conn);
	conn->handle = HANDLE;
	bt_conn_set_state(conn, BT_CONN_CONNECTED);
	bt_l2cap_connected(conn);

	chan_connect(&chan, &chan_ops, PEER_CID);

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

/* Given by the status callback once the channel is out of credits */
static os_sem_t paused_sem;
/* What the status callback saw: the credits left and the command's result */
static bt_atomic_val_t paused_credits;
static int paused_err;

/* Out of credits, wait for the Controller, as bt_conn_disconnect() does */
static void chan_status(struct bt_l2cap_chan *ch, bt_atomic_t *status)
{
	if (bt_atomic_test_bit(status, BT_L2CAP_STATUS_OUT)) {
		return;
	}

	paused_credits = bt_atomic_get(&BT_L2CAP_LE_CHAN(ch)->tx.credits);
	paused_err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_LOCAL_VERSION_INFO, NULL, NULL);
	bt_test_require(osi_sem_give(&paused_sem));
}

/* The status callback of a channel that runs out of credits may wait for the Controller */
static void host_l2cap_le_tx__test_status_sync(void **state)
{
	static const struct bt_l2cap_chan_ops ops = {
		.recv = chan_recv,
		.status = chan_status,
		.connected = chan_connected,
		.released = chan_disconnected,
	};
	static struct bt_l2cap_le_chan status_chan;
	/* Not a CID of the other test */
	const uint16_t peer_cid = PEER_CID + 2U;
	struct net_buf *buf;
	uint8_t ident;

	status_chan.chan.ops = &ops;
	status_chan.rx.mtu = LE_MIN_MTU;
	assert_int_equal(bt_l2cap_chan_connect(conn, &status_chan.chan, PSM), 0);

	/* Credit for one K-frame */
	ident = tx_sig(BT_L2CAP_LE_CONN_REQ);
	buf = sig_cmd(BT_L2CAP_LE_CONN_RSP, ident);
	net_buf_add_le16(buf, peer_cid);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, LE_MIN_MTU);
	net_buf_add_le16(buf, 1U);
	net_buf_add_le16(buf, BT_L2CAP_LE_SUCCESS);
	recv_sig(buf);
	assert_int_equal(osi_sem_take(&connected_sem, TX_TIMEOUT), 0);
	assert_int_equal(status_chan.state, BT_L2CAP_CONNECTED);

	buf = net_buf_alloc(&sdu_pool, BT_NO_WAIT);
	assert_non_null(buf);
	net_buf_reserve(buf, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
	(void)net_buf_add_mem(buf, DATA, strlen(DATA));
	assert_int_equal(bt_l2cap_chan_send(&status_chan.chan, buf), 0);

	assert_int_equal(osi_sem_take(&paused_sem, TX_TIMEOUT), 0);
	assert_int_equal(paused_credits, 0);
	assert_int_equal(paused_err, 0);

	buf = tx(peer_cid);
	assert_int_equal(net_buf_pull_le16(buf), strlen(DATA));
	assert_memory_equal(buf->data, DATA, strlen(DATA));
	net_buf_unref(buf);
}

/* Nothing else may follow what a test expected the Host to send */
static void tx_none(void)
{
	assert_null(bt_fifo_get(&sent, TX_TIMEOUT));
}

/* The next PDU must be a Disconnection Request, which the peer accepts. Returns
 * the destination CID of the request.
 */
static uint16_t disconn_accept(void)
{
	struct bt_l2cap_disconn_req *req;
	struct bt_l2cap_sig_hdr *sig;
	uint16_t dcid, scid;
	struct net_buf *buf;
	uint8_t ident;

	buf = tx(BT_L2CAP_CID_LE_SIG);
	sig = net_buf_pull_mem(buf, sizeof(*sig));
	assert_int_equal(sig->code, BT_L2CAP_DISCONN_REQ);
	ident = sig->ident;
	req = net_buf_pull_mem(buf, sizeof(*req));
	dcid = sys_le16_to_cpu(req->dcid);
	scid = sys_le16_to_cpu(req->scid);
	net_buf_unref(buf);

	buf = sig_cmd(BT_L2CAP_DISCONN_RSP, ident);
	net_buf_add_le16(buf, dcid);
	net_buf_add_le16(buf, scid);
	recv_sig(buf);
	assert_int_equal(osi_sem_take(&released_sem, TX_TIMEOUT), 0);

	return dcid;
}

/* A channel is disconnected while the Disconnection Request of another one waits
 * for the controller
 */
static void host_l2cap_le_tx__test_disconnect_two(void **state)
{
	static struct bt_l2cap_le_chan chans[2];
	/* Not CIDs of the other tests */
	const uint16_t peer_cid = PEER_CID + 3U;

	ARRAY_FOR_EACH(chans, i) {
		chan_connect(&chans[i], &chan_ops, peer_cid + i);
	}

	/* A response takes the controller's buffer until release() */
	hold = true;
	recv_unknown_conn_req(PEER_IDENT + 3U);
	assert_int_equal(tx_sig(BT_L2CAP_LE_CONN_RSP), PEER_IDENT + 3U);

	ARRAY_FOR_EACH(chans, i) {
		assert_int_equal(bt_l2cap_chan_disconnect(&chans[i].chan), 0);
	}

	release();

	ARRAY_FOR_EACH(chans, i) {
		assert_int_equal(disconn_accept(), peer_cid + i);
		assert_int_equal(chans[i].state, BT_L2CAP_DISCONNECTED);
	}

	tx_none();
}

/* A channel that waits for a response to a request of its own keeps the request
 * and its timer when its Disconnection Request finds no buffer
 */
static void host_l2cap_le_tx__test_disconnect_reconf_pending(void **state)
{
	static struct bt_l2cap_le_chan chans[2];
	struct bt_l2cap_chan *reconf[] = { &chans[1].chan, NULL };
	/* Not CIDs of the other tests */
	const uint16_t peer_cid = PEER_CID + 7U;
	struct net_buf *buf;
	uint8_t ident;

	ARRAY_FOR_EACH(chans, i) {
		chan_connect(&chans[i], &chan_ops, peer_cid + i);
	}

	assert_int_equal(bt_l2cap_ecred_chan_reconfigure(reconf, LE_MIN_MTU), 0);
	ident = tx_sig(BT_L2CAP_ECRED_RECONF_REQ);

	/* The Disconnection Request of the first channel waits for the controller */
	hold = true;
	recv_unknown_conn_req(PEER_IDENT + 5U);
	assert_int_equal(tx_sig(BT_L2CAP_LE_CONN_RSP), PEER_IDENT + 5U);
	assert_int_equal(bt_l2cap_chan_disconnect(&chans[0].chan), 0);

	assert_int_equal(bt_l2cap_chan_disconnect(&chans[1].chan), -ENOMEM);
	assert_int_equal(chans[1].state, BT_L2CAP_CONNECTED);
	assert_true(bt_work_delayable_is_pending(&chans[1].rtx_work));

	release();
	assert_int_equal(disconn_accept(), peer_cid);

	buf = sig_cmd(BT_L2CAP_ECRED_RECONF_RSP, ident);
	net_buf_add_le16(buf, BT_L2CAP_RECONF_SUCCESS);
	recv_sig(buf);

	assert_int_equal(bt_l2cap_chan_disconnect(&chans[1].chan), 0);
	assert_int_equal(disconn_accept(), peer_cid + 1U);
	assert_int_equal(chans[1].state, BT_L2CAP_DISCONNECTED);

	tx_none();
}

/* The pool that the Host allocates its signalling PDUs from, while a test holds all of it */
static struct net_buf *host_pdus[CONFIG_BT_L2CAP_TX_BUF_COUNT];

static void host_pdus_take(void)
{
	ARRAY_FOR_EACH(host_pdus, i) {
		host_pdus[i] = bt_conn_create_pdu_timeout(NULL, 0, BT_NO_WAIT);
		assert_non_null(host_pdus[i]);
	}
}

static void host_pdus_give(void)
{
	ARRAY_FOR_EACH(host_pdus, i) {
		net_buf_unref(host_pdus[i]);
		host_pdus[i] = NULL;
	}
}

/* The SDU that test_credits_no_buf receives */
NET_BUF_POOL_FIXED_DEFINE(rx_sdu_pool, 1, LE_MIN_MTU, sizeof(uint16_t), NULL);

static struct net_buf *chan_alloc_buf(struct bt_l2cap_chan *ch)
{
	return net_buf_alloc(&rx_sdu_pool, BT_NO_WAIT);
}

/* What the disconnection by chan_shutdown_disconnect() returned */
static int shutdown_disconnect_err;

/* As an application may react to the shutdown */
static void chan_shutdown_disconnect(struct bt_l2cap_chan *ch, bt_atomic_t *status)
{
	if (bt_atomic_test_bit(status, BT_L2CAP_STATUS_SHUTDOWN)) {
		shutdown_disconnect_err = bt_l2cap_chan_disconnect(ch);
		bt_test_require(osi_sem_give(&shutdown_sem));
	}
}

/* A channel that cannot give the peer credits for the rest of an SDU is
 * disconnected, once also when its status callback disconnects it as well
 */
static void host_l2cap_le_tx__test_credits_no_buf(void **state)
{
	static const struct bt_l2cap_chan_ops ops = {
		.alloc_buf = chan_alloc_buf,
		.recv = chan_recv,
		.status = chan_shutdown_disconnect,
		.connected = chan_connected,
		.released = chan_disconnected,
	};
	static struct bt_l2cap_le_chan sdu_chan;
	/* Not a CID of the other tests */
	const uint16_t peer_cid = PEER_CID + 5U;
	struct net_buf *buf;

	chan_connect(&sdu_chan, &ops, peer_cid);

	/* No buffer is left for an LE Flow Control Credit packet */
	host_pdus_take();
	/* Keep the resource-exhaustion precondition independent of TX scheduling. */
	keep_disconnect = true;

	/* The first K-frame of an SDU that needs more */
	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_FOREVER);
	(void)net_buf_add(buf, sizeof(struct bt_hci_acl_hdr) + sizeof(struct bt_l2cap_hdr));
	net_buf_add_le16(buf, LE_MIN_MTU);
	(void)net_buf_add_mem(buf, DATA, strlen(DATA));
	recv_pdu(buf, sdu_chan.rx.cid);

	/* Until the Host gives up waiting for a buffer for the credits */
	assert_int_equal(osi_sem_take(&shutdown_sem, BT_L2CAP_RTX_TIMEOUT + TX_TIMEOUT), 0);

	/* The request sent first still holds the disconnect buffer */
	assert_int_equal(shutdown_disconnect_err, -ENOMEM);
	assert_int_equal(disconn_accept(), peer_cid);
	assert_int_equal(sdu_chan.state, BT_L2CAP_DISCONNECTED);
	assert_non_null(kept_disconnect);
	keep_disconnect = false;
	net_buf_unref(kept_disconnect);
	kept_disconnect = NULL;

	host_pdus_give();
	tx_none();
}


static void chan_disconnected(struct bt_l2cap_chan *ch)
{
	bt_test_require(osi_sem_give(&released_sem));
}

/* A channel whose peer requests the disconnection is released also when the
 * Host has no buffer for the response
 */
static void host_l2cap_le_tx__test_disconn_rsp_no_buf(void **state)
{
	static const struct bt_l2cap_chan_ops ops = {
		.recv = chan_recv,
		.released = chan_disconnected,
		.connected = chan_connected,
	};
	static struct bt_l2cap_le_chan peer_chan;
	/* Not a CID of the other tests */
	const uint16_t peer_cid = PEER_CID + 6U;
	struct net_buf *buf;

	chan_connect(&peer_chan, &ops, peer_cid);

	host_pdus_take();

	buf = sig_cmd(BT_L2CAP_DISCONN_REQ, PEER_IDENT + 4U);
	net_buf_add_le16(buf, peer_chan.rx.cid);
	net_buf_add_le16(buf, peer_cid);
	recv_sig(buf);

	/* Until the Host gives up waiting for a buffer for the response */
	assert_int_equal(osi_sem_take(&released_sem, BT_L2CAP_RTX_TIMEOUT + TX_TIMEOUT), 0);
	assert_int_equal(peer_chan.state, BT_L2CAP_DISCONNECTED);

	host_pdus_give();
	tx_none();
}

static void storage_init_7767618e85(void)
{
	net_buf_pool_init(&sdu_pool);
	net_buf_pool_init(&sent_pool);
	net_buf_pool_init(&rx_sdu_pool);
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

	bt_test_require(osi_sem_init(&connected_sem, 0, 1));
	bt_test_require(osi_sem_init(&released_sem, 0, 1));
	bt_test_require(osi_sem_init(&paused_sem, 0, 1));
	bt_test_require(osi_sem_init(&shutdown_sem, 0, 1));
	bt_test_require(bt_conn_init());
	migrated_pools_init();
	test_transport_init();
	storage_init_7767618e85();
	*state = setup();
	return 0;
}



static const struct CMUnitTest test_cases_host_l2cap_le_tx[] = {
	cmocka_unit_test(host_l2cap_le_tx__test_no_credits),
	cmocka_unit_test(host_l2cap_le_tx__test_status_sync),
	cmocka_unit_test(host_l2cap_le_tx__test_disconnect_two),
	cmocka_unit_test(host_l2cap_le_tx__test_disconnect_reconf_pending),
	cmocka_unit_test(host_l2cap_le_tx__test_credits_no_buf),
	cmocka_unit_test(host_l2cap_le_tx__test_disconn_rsp_no_buf),
};
BT_TEST_SUITE(host_l2cap_le_tx, test_cases_host_l2cap_le_tx, bt_test_host_l2cap_le_tx_setup, NULL);
