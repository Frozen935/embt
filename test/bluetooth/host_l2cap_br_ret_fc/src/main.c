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
#include <embt/bluetooth/classic/l2cap_br.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/l2cap.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/base/atomic.h>
#include <embt/base/byteorder.h>
#include <embt/base/crc.h>
#include <embt/base/util.h>
#include <embt/base/work.h>
#include "host/conn_internal.h"
#include "host/classic/l2cap_br_internal.h"


#define GENERIC_RP_SIZE 64U
#define ACL_MAX_LEN     310U
#define LE_ACL_MAX_LEN  27U
#define LE_ACL_MAX_NUM  3U
#define HANDLE          0x0001U
#define OTHER_HANDLE    0x0002U

/* Headers of an ACL packet from the controller, after its H:4 packet type */
#define ACL_HDR_OFFSET   1U
#define L2CAP_HDR_OFFSET (ACL_HDR_OFFSET + sizeof(struct bt_hci_acl_hdr))
#define SIG_HDR_OFFSET   (L2CAP_HDR_OFFSET + sizeof(struct bt_l2cap_hdr))

#define DATA         "data"
#define SDP_PSM      0x0001U
#define PEER_CID     0x0040U
#define PEER_IDENT   0x80U
#define PEER_MPS     48U
#define MIN_MTU      48U
#define FEAT_ENH_RET BIT(3)
/* x^16 + x^15 + x^2 + 1, bit-reversed */
#define FCS_POLY     0xa001U

/* Shorter than the retransmission timeout, so no poll is sent meanwhile */
#define TX_TIMEOUT BT_MSEC(100)
/* An expected packet may follow a 100 ms allocation retry plus native scheduling.
 * Keep the observation budget below retransmission so that no poll is introduced.
 */
#define TX_COMPLETION_TIMEOUT BT_SECONDS(1)
BUILD_ASSERT(1000 < CONFIG_BT_L2CAP_BR_RET_TIMEOUT);

/* An SDU per case and two for the window case, which a case that fails keeps */
NET_BUF_POOL_FIXED_DEFINE(sdu_pool, 6, BT_L2CAP_CHAN_SEND_RESERVE + sizeof(DATA),
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);
/* The PDUs that the Host sent and the test has not taken yet: at most a response per
 * signalling buffer, the Disconnection Request and the I-frame that a wrong Host sends
 * again
 */
NET_BUF_POOL_FIXED_DEFINE(sent_pool, CONFIG_BT_L2CAP_SIG_TX_BUF_COUNT + 2, ACL_MAX_LEN, 0, NULL);

static struct bt_conn *conn;

/* The L2CAP PDUs that the Host sent */
static BT_FIFO_DEFINE(sent);

/* The controller has one ACL buffer, which it reports free again unless the
 * test holds it, as for a peer that does not acknowledge at the baseband.
 * Counted per connection handle.
 */
static bt_atomic_t unreported[2];
static bool hold;

/* The buffers of the Host that the driver keeps, as one sending them later does */
static BT_FIFO_DEFINE(kept);
static bool keep;

static os_sem_t echo_sem;

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
	/* LE supported, BR/EDR supported */
	rp->features[4] = BIT(6);
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

static void read_buffer_size(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_read_buffer_size *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	(void)memset(rp, 0, sizeof(*rp));
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->acl_max_len = sys_cpu_to_le16(ACL_MAX_LEN);
	rp->acl_max_num = sys_cpu_to_le16(1U);
}

static void le_read_buffer_size(struct net_buf **evt, uint16_t opcode)
{
	struct bt_hci_rp_le_read_buffer_size *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->le_max_len = sys_cpu_to_le16(LE_ACL_MAX_LEN);
	rp->le_max_num = LE_ACL_MAX_NUM;
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
	case BT_HCI_OP_READ_BUFFER_SIZE:
		read_buffer_size(&evt, opcode);
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
	bt_atomic_val_t count[ARRAY_SIZE(unreported)];
	struct bt_hci_evt_num_completed_packets *ev;
	struct bt_hci_handle_count *hc;
	uint8_t num_handles = 0U;
	struct net_buf *evt;

	ARG_UNUSED(work);

	ARRAY_FOR_EACH(unreported, i) {
		count[i] = bt_atomic_clear(&unreported[i]);
		if (count[i] != 0) {
			num_handles++;
		}
	}

	if (num_handles == 0U) {
		return;
	}

	evt = bt_buf_get_evt(BT_HCI_EVT_NUM_COMPLETED_PACKETS, false, BT_FOREVER);
	evt_create(evt, BT_HCI_EVT_NUM_COMPLETED_PACKETS,
		   sizeof(*ev) + (num_handles * sizeof(*hc)));
	ev = net_buf_add(evt, sizeof(*ev));
	ev->num_handles = num_handles;

	ARRAY_FOR_EACH(unreported, i) {
		if (count[i] != 0) {
			hc = net_buf_add(evt, sizeof(*hc));
			hc->handle = sys_cpu_to_le16(HANDLE + i);
			hc->count = sys_cpu_to_le16((uint16_t)count[i]);
		}
	}

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
	uint16_t handle;

	/* Every PDU fits a single ACL packet */
	hdr = net_buf_pull_mem(acl, sizeof(*hdr));
	handle = bt_acl_handle(sys_le16_to_cpu(hdr->handle));
	assert_true((handle == HANDLE) || (handle == OTHER_HANDLE));

	pdu = net_buf_alloc(&sent_pool, BT_NO_WAIT);
	assert_non_null(pdu);
	(void)net_buf_add_mem(pdu, acl->data, acl->len);
	bt_fifo_put(&sent, pdu);

	(void)bt_atomic_inc(&unreported[handle - HANDLE]);
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

		if (keep) {
			bt_fifo_put(&kept, buf);
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


static void unkeep(void)
{
	struct net_buf *buf;

	keep = false;

	while ((buf = bt_fifo_get(&kept, BT_NO_WAIT)) != NULL) {
		net_buf_unref(buf);
	}
}

/* The next L2CAP PDU that the Host sent */
static struct net_buf *tx(void)
{
	struct net_buf *buf;
	struct bt_work_sync sync = {0};

	buf = bt_fifo_get(&sent, TX_COMPLETION_TIMEOUT);
	assert_non_null(buf);
	/* FIFO publication precedes the driver's return. Observe that TX pass and its
	 * completion work without draining a queue that still has active producers.
	 */
	bt_dev_lock();
	bt_dev_unlock();
	(void)bt_work_flush(&completed_work, &sync);

	return buf;
}

/* The next PDU must be a signalling command with the given code. Copies the
 * start of its data and returns its identifier.
 */
static uint8_t tx_sig(uint8_t code, void *data, size_t len)
{
	struct bt_l2cap_sig_hdr *sig;
	struct bt_l2cap_hdr *hdr;
	struct net_buf *buf;
	uint8_t ident;

	buf = tx();

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	assert_int_equal(sys_le16_to_cpu(hdr->cid), BT_L2CAP_CID_BR_SIG);

	sig = net_buf_pull_mem(buf, sizeof(*sig));
	assert_int_equal(sig->code, code);
	ident = sig->ident;

	if (len > 0U) {
		assert_true(buf->len >= len);
		(void)memcpy(data, buf->data, len);
	}

	net_buf_unref(buf);

	return ident;
}

/* An ACL packet from the controller, up to its L2CAP header */
static struct net_buf *rx_frame(void)
{
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_FOREVER);
	(void)net_buf_add(buf, sizeof(struct bt_hci_acl_hdr));

	return buf;
}

/* Deliver the packet and let the Host handle it, which takes no simulated time */
static void recv_frame(struct net_buf *buf, uint16_t handle)
{
	struct bt_hci_acl_hdr *acl = (void *)&buf->data[ACL_HDR_OFFSET];

	acl->handle = sys_cpu_to_le16(bt_acl_handle_pack(handle, BT_ACL_START));
	acl->len = sys_cpu_to_le16(buf->len - L2CAP_HDR_OFFSET);

	bt_hci_recv(&test_driver, buf);
	assert_int_equal(bt_host_work_drain(), 0);
}

static struct net_buf *sig_cmd(uint8_t code, uint8_t ident)
{
	struct bt_l2cap_sig_hdr *sig;
	struct net_buf *buf;

	buf = rx_frame();
	(void)net_buf_add(buf, sizeof(struct bt_l2cap_hdr));

	sig = net_buf_add(buf, sizeof(*sig));
	sig->code = code;
	sig->ident = ident;

	return buf;
}

static void recv_sig_on(struct net_buf *buf, uint16_t handle)
{
	struct bt_l2cap_sig_hdr *sig = (void *)&buf->data[SIG_HDR_OFFSET];
	struct bt_l2cap_hdr *hdr = (void *)&buf->data[L2CAP_HDR_OFFSET];

	sig->len = sys_cpu_to_le16(buf->len - SIG_HDR_OFFSET - sizeof(*sig));
	hdr->len = sys_cpu_to_le16(buf->len - SIG_HDR_OFFSET);
	hdr->cid = sys_cpu_to_le16(BT_L2CAP_CID_BR_SIG);

	recv_frame(buf, handle);
}

static void recv_sig(struct net_buf *buf)
{
	recv_sig_on(buf, HANDLE);
}

static void recv_s_frame(struct bt_l2cap_br_chan *chan, uint8_t s, uint8_t req_seq)
{
	struct net_buf *buf;
	uint16_t fcs;

	buf = rx_frame();
	net_buf_add_le16(buf, BT_L2CAP_ENH_CONTROL_SIZE + BT_L2CAP_FCS_SIZE);
	net_buf_add_le16(buf, chan->rx.cid);
	net_buf_add_le16(buf, BT_L2CAP_S_FRAME_ENH_CONTROL_SET(s, 0U, 0U, req_seq));

	fcs = crc16_reflect(FCS_POLY, 0U, &buf->data[L2CAP_HDR_OFFSET],
			    buf->len - L2CAP_HDR_OFFSET);
	net_buf_add_le16(buf, fcs);

	recv_frame(buf, HANDLE);
}

/* The responses to Information Requests on a link take every signalling buffer */
static void take_sig_bufs(uint16_t handle)
{
	struct net_buf *buf;

	for (uint8_t i = 0U; i < CONFIG_BT_L2CAP_SIG_TX_BUF_COUNT; i++) {
		buf = sig_cmd(BT_L2CAP_INFO_REQ, PEER_IDENT);
		net_buf_add_le16(buf, BT_L2CAP_INFO_FEAT_MASK);
		recv_sig_on(buf, handle);
	}
}

static void expect_info_rsps(void)
{
	for (uint8_t i = 0U; i < CONFIG_BT_L2CAP_SIG_TX_BUF_COUNT; i++) {
		assert_int_equal(tx_sig(BT_L2CAP_INFO_RSP, NULL, 0U), PEER_IDENT);
	}
}

static int chan_recv(struct bt_l2cap_chan *ch, struct net_buf *buf)
{
	return 0;
}

static const struct bt_l2cap_chan_ops chan_ops = {
	.recv = chan_recv,
};

/* Connect a channel in Enhanced Retransmission Mode */
static void chan_connect(struct bt_l2cap_br_chan *chan, uint16_t peer_cid, uint8_t window,
			 uint8_t max_transmit)
{
	struct net_buf *buf;
	uint8_t ident;

	chan->chan.ops = &chan_ops;
	chan->rx.mtu = MIN_MTU;
	chan->rx.mode = BT_L2CAP_BR_LINK_MODE_ERET;
	chan->rx.fcs = BT_L2CAP_BR_FCS_16BIT;
	chan->rx.max_transmit = 1U;
	assert_int_equal(bt_l2cap_chan_connect(conn, &chan->chan, SDP_PSM), 0);

	ident = tx_sig(BT_L2CAP_CONN_REQ, NULL, 0U);
	buf = sig_cmd(BT_L2CAP_CONN_RSP, ident);
	net_buf_add_le16(buf, peer_cid);
	net_buf_add_le16(buf, chan->rx.cid);
	net_buf_add_le16(buf, BT_L2CAP_BR_SUCCESS);
	net_buf_add_le16(buf, 0U);
	recv_sig(buf);

	ident = tx_sig(BT_L2CAP_CONF_REQ, NULL, 0U);

	buf = sig_cmd(BT_L2CAP_CONF_REQ, PEER_IDENT);
	net_buf_add_le16(buf, chan->rx.cid);
	net_buf_add_le16(buf, 0U);
	net_buf_add_u8(buf, BT_L2CAP_CONF_OPT_RET_FC);
	net_buf_add_u8(buf, sizeof(struct bt_l2cap_conf_opt_ret_fc));
	net_buf_add_u8(buf, BT_L2CAP_BR_LINK_MODE_ERET);
	/* TxWindow size, MaxTransmit, timeouts left to the receiver, MPS */
	net_buf_add_u8(buf, window);
	net_buf_add_u8(buf, max_transmit);
	net_buf_add_le16(buf, 0U);
	net_buf_add_le16(buf, 0U);
	net_buf_add_le16(buf, PEER_MPS);
	recv_sig(buf);

	assert_int_equal(tx_sig(BT_L2CAP_CONF_RSP, NULL, 0U), PEER_IDENT);

	buf = sig_cmd(BT_L2CAP_CONF_RSP, ident);
	net_buf_add_le16(buf, chan->rx.cid);
	net_buf_add_le16(buf, 0U);
	net_buf_add_le16(buf, BT_L2CAP_CONF_SUCCESS);
	recv_sig(buf);

	assert_int_equal(chan->state, BT_L2CAP_CONNECTED);
	assert_int_equal(chan->tx.mode, BT_L2CAP_BR_LINK_MODE_ERET);
	assert_int_equal(chan->tx.max_transmit, max_transmit);
	assert_int_equal(chan->tx.max_window, window);
	assert_int_equal(chan->rx.fcs, BT_L2CAP_BR_FCS_16BIT);
}

static void send_sdu(struct bt_l2cap_br_chan *chan)
{
	struct net_buf *buf;

	buf = net_buf_alloc(&sdu_pool, BT_NO_WAIT);
	assert_non_null(buf);
	net_buf_reserve(buf, BT_L2CAP_CHAN_SEND_RESERVE);
	(void)net_buf_add_mem(buf, DATA, strlen(DATA));

	assert_int_equal(bt_l2cap_chan_send(&chan->chan, buf), 0);
}

/* Send an I-frame, which the peer asks for again: a second transmission would
 * exceed a MaxTransmit of 1. The controller reports the I-frame complete only on
 * release(), so that the Host decides that the channel is lost after what the
 * test sets up meanwhile.
 */
static void lose_chan(struct bt_l2cap_br_chan *chan, uint16_t peer_cid)
{
	struct bt_l2cap_hdr *hdr;
	struct net_buf *buf;

	hold = true;
	send_sdu(chan);

	buf = tx();
	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	assert_int_equal(sys_le16_to_cpu(hdr->cid), peer_cid);
	net_buf_unref(buf);

	recv_s_frame(chan, BT_L2CAP_CONTROL_S_SREJ, 0U);
}

static void expect_disconn_req(struct bt_l2cap_br_chan *chan, uint16_t peer_cid)
{
	struct bt_l2cap_disconn_req req;

	uint8_t ident = tx_sig(BT_L2CAP_DISCONN_REQ, &req, sizeof(req));

	assert_int_equal(ident, chan->ident);
	assert_int_equal(sys_le16_to_cpu(req.dcid), peer_cid);
	assert_int_equal(sys_le16_to_cpu(req.scid), chan->rx.cid);
	assert_int_equal(chan->state, BT_L2CAP_DISCONNECTING);
}

/* The peer agrees, which removes the channel */
static void recv_disconn_rsp(struct bt_l2cap_br_chan *chan, uint16_t peer_cid)
{
	struct net_buf *buf;

	buf = sig_cmd(BT_L2CAP_DISCONN_RSP, chan->ident);
	net_buf_add_le16(buf, peer_cid);
	net_buf_add_le16(buf, chan->rx.cid);
	recv_sig(buf);
}

static void echo_req(struct bt_conn *c, uint8_t identifier, struct net_buf *buf)
{
	osi_sem_give(&echo_sem);
}

static struct bt_l2cap_br_echo_cb echo_cb = {
	.req = echo_req,
};

static void *setup(void)
{
	static const bt_addr_t addr = { .val = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };
	struct net_buf *buf;
	uint8_t ident;

	assert_int_equal(bt_enable(NULL), 0);
	assert_int_equal(bt_l2cap_br_echo_cb_register(&echo_cb), 0);

	/* As on Connection Complete */
	conn = bt_conn_add_br(&addr);
	assert_non_null(conn);
	conn->handle = HANDLE;
	bt_conn_set_state(conn, BT_CONN_CONNECTED);
	bt_l2cap_br_connected(conn);

	ident = tx_sig(BT_L2CAP_INFO_REQ, NULL, 0U);
	buf = sig_cmd(BT_L2CAP_INFO_RSP, ident);
	net_buf_add_le16(buf, BT_L2CAP_INFO_FEAT_MASK);
	net_buf_add_le16(buf, BT_L2CAP_INFO_SUCCESS);
	net_buf_add_le32(buf, FEAT_ENH_RET);
	recv_sig(buf);

	return NULL;
}



/* The fallback of a signalling buffer allocation is empty */
static void take_acl_bufs(sys_slist_t *taken)
{
	struct net_buf *buf;

	sys_slist_init(taken);

	while ((buf = bt_conn_create_pdu_timeout(NULL, 0U, BT_NO_WAIT)) != NULL) {
		net_buf_slist_put(taken, buf);
	}
}

static void give_acl_bufs(sys_slist_t *taken)
{
	struct net_buf *buf;

	while ((buf = net_buf_slist_get(taken)) != NULL) {
		net_buf_unref(buf);
	}
}

/* A channel lost to MaxTransmit is disconnected, although signalling responses
 * queued behind it hold every signalling buffer: the TX processor sends them first.
 */
static void l2cap_br_ret_fc__test_max_transmit(void **cm_state)
{
	(void)cm_state;
	static struct bt_l2cap_br_chan chan;

	chan_connect(&chan, PEER_CID, 1U, 1U);
	lose_chan(&chan, PEER_CID);
	take_sig_bufs(HANDLE);

	release();

	/* Not the I-frame again */
	expect_info_rsps();

	expect_disconn_req(&chan, PEER_CID);
	recv_disconn_rsp(&chan, PEER_CID);
}

/* The Disconnection Request of a lost channel waits for no buffer, neither on
 * the TX processor nor on the Bluetooth work queue: it follows once one is free.
 */
static void l2cap_br_ret_fc__test_max_transmit_no_buf(void **cm_state)
{
	(void)cm_state;
	static struct bt_l2cap_br_chan chan;
	struct net_buf *buf;
	sys_slist_t taken;

	chan_connect(&chan, PEER_CID + 1U, 1U, 1U);
	lose_chan(&chan, PEER_CID + 1U);
	take_sig_bufs(HANDLE);
	take_acl_bufs(&taken);

	/* The driver keeps the signalling buffers of the responses once sent */
	keep = true;
	release();

	expect_info_rsps();

	/* The Bluetooth work queue still runs */
	buf = sig_cmd(BT_L2CAP_ECHO_REQ, PEER_IDENT);
	recv_sig(buf);
	assert_int_equal(osi_sem_take(&echo_sem, TX_TIMEOUT), 0);

	assert_null(bt_fifo_get(&sent, TX_TIMEOUT));
	assert_int_equal(chan.state, BT_L2CAP_CONNECTED);

	unkeep();
	expect_disconn_req(&chan, PEER_CID + 1U);
	recv_disconn_rsp(&chan, PEER_CID + 1U);

	give_acl_bufs(&taken);
}

/* Nothing more is sent on a lost channel, although its window has room for a
 * pending SDU.
 */
static void l2cap_br_ret_fc__test_max_transmit_stops_tx(void **cm_state)
{
	(void)cm_state;
	static struct bt_l2cap_br_chan chan;

	chan_connect(&chan, PEER_CID + 2U, 2U, 1U);
	lose_chan(&chan, PEER_CID + 2U);
	send_sdu(&chan);
	take_sig_bufs(HANDLE);

	release();

	expect_info_rsps();
	expect_disconn_req(&chan, PEER_CID + 2U);
	assert_null(bt_fifo_get(&sent, TX_TIMEOUT));

	recv_disconn_rsp(&chan, PEER_CID + 2U);
}

/* The largest MaxTransmit allows that many transmissions, and no more */
static void l2cap_br_ret_fc__test_max_transmit_255(void **cm_state)
{
	(void)cm_state;
	static struct bt_l2cap_br_chan chan;
	struct bt_l2cap_hdr *hdr;
	struct net_buf *buf;

	chan_connect(&chan, PEER_CID + 3U, 1U, UINT8_MAX);
	send_sdu(&chan);

	for (uint16_t i = 0U; i < UINT8_MAX; i++) {
		buf = tx();
		hdr = net_buf_pull_mem(buf, sizeof(*hdr));
		assert_int_equal(sys_le16_to_cpu(hdr->cid), PEER_CID + 3U);
		net_buf_unref(buf);

		recv_s_frame(&chan, BT_L2CAP_CONTROL_S_SREJ, 0U);
	}

	expect_disconn_req(&chan, PEER_CID + 3U);
	recv_disconn_rsp(&chan, PEER_CID + 3U);
}

/* A lost channel does not hold up another link, whose queued signalling responses
 * hold every signalling buffer that the Disconnection Request needs.
 */
static void l2cap_br_ret_fc__test_max_transmit_other_link(void **cm_state)
{
	(void)cm_state;
	static const bt_addr_t addr = { .val = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16 } };
	static struct bt_l2cap_br_chan chan;
	struct bt_conn *other;
	struct net_buf *buf;
	sys_slist_t taken;
	uint8_t ident;

	if (CONFIG_BT_MAX_CONN < 2) {
		skip();
	}

	other = bt_conn_add_br(&addr);
	assert_non_null(other);
	other->handle = OTHER_HANDLE;
	bt_conn_set_state(other, BT_CONN_CONNECTED);
	bt_l2cap_br_connected(other);

	ident = tx_sig(BT_L2CAP_INFO_REQ, NULL, 0U);
	buf = sig_cmd(BT_L2CAP_INFO_RSP, ident);
	net_buf_add_le16(buf, BT_L2CAP_INFO_FEAT_MASK);
	net_buf_add_le16(buf, BT_L2CAP_INFO_SUCCESS);
	net_buf_add_le32(buf, 0U);
	recv_sig_on(buf, OTHER_HANDLE);

	chan_connect(&chan, PEER_CID + 4U, 1U, 1U);
	lose_chan(&chan, PEER_CID + 4U);
	take_sig_bufs(OTHER_HANDLE);
	take_acl_bufs(&taken);

	release();

	expect_info_rsps();
	expect_disconn_req(&chan, PEER_CID + 4U);
	recv_disconn_rsp(&chan, PEER_CID + 4U);

	give_acl_bufs(&taken);
}

static void storage_init_18ffcfc67e(void)
{
	net_buf_pool_init(&sdu_pool);
	net_buf_pool_init(&sent_pool);
	bt_fifo_init(&sent);
	bt_fifo_init(&kept);
	bt_test_require(osi_sem_init(&echo_sem, 0, 1));
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&sdu_pool);
	net_buf_pool_init(&sent_pool);
}

int bt_test_l2cap_br_ret_fc_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_pools_init();
	test_transport_init();
	storage_init_18ffcfc67e();
	*state = setup();
	return 0;
}



static const struct CMUnitTest test_cases_l2cap_br_ret_fc[] = {
	cmocka_unit_test(l2cap_br_ret_fc__test_max_transmit),
	cmocka_unit_test(l2cap_br_ret_fc__test_max_transmit_no_buf),
	cmocka_unit_test(l2cap_br_ret_fc__test_max_transmit_stops_tx),
	cmocka_unit_test(l2cap_br_ret_fc__test_max_transmit_255),
	cmocka_unit_test(l2cap_br_ret_fc__test_max_transmit_other_link),
};
BT_TEST_SUITE(l2cap_br_ret_fc, test_cases_l2cap_br_ret_fc, bt_test_l2cap_br_ret_fc_setup, NULL);
