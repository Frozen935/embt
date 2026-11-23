#include "support/test_support.h"
static const struct bt_hci_transport test_driver;
#include "host/hci_core.h"
/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/gap.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/l2cap.h>
#include <embt/bluetooth/uuid.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/base/atomic.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include "host/att_internal.h"
#include "host/conn_internal.h"
#include "host/l2cap_internal.h"


/* Large enough for the return parameters of initialization commands that are
 * irrelevant to this test
 */
#define GENERIC_RP_SIZE 64U

#define LE_ACL_MAX_LEN 27U
#define LE_ACL_MAX_NUM 3U

#define TEST_CONN_HANDLE 0x0001U

BT_GATT_SERVICE_DEFINE(test_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0xfff0)),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0xfff1),
			       BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_INDICATE, BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
);

/* The notifications, indications and signalling PDUs that the controller got,
 * and the data packets it did not expect
 */
static bt_atomic_t notifications;
static bt_atomic_t indications;
static bt_atomic_t sig_pdus;
static os_sem_t sig_sent_sem;
static bt_atomic_t acl_unexpected;

/* Whether the link is up, and the data packets that the controller holds on it
 * until it reports them as completed
 */
static bt_atomic_t link_up;
static bt_atomic_t acl_pending;

/* The driver's send() of the next data packet blocks until the test releases it */
static bt_atomic_t send_block;
static os_sem_t send_blocked_sem;
static os_sem_t send_release_sem;

/* The driver keeps the next data packet, so that its buffer is released, and
 * the Host completes the PDU in it, only when the test lets it go
 */
static bt_atomic_t acl_hold;
static struct net_buf *acl_held;
static os_sem_t acl_held_sem;

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

/* A data packet from the Host must carry a notification, an indication or a
 * signalling PDU
 */
static void acl_handle(struct net_buf *buf)
{
	struct bt_hci_acl_hdr *hdr;
	uint16_t len;
	uint16_t cid;

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));

	if (bt_acl_handle(sys_le16_to_cpu(hdr->handle)) != TEST_CONN_HANDLE ||
	    buf->len < BT_L2CAP_HDR_SIZE) {
		(void)bt_atomic_inc(&acl_unexpected);
		return;
	}

	len = net_buf_pull_le16(buf);
	cid = net_buf_pull_le16(buf);

	if (len != buf->len || buf->len == 0U) {
		(void)bt_atomic_inc(&acl_unexpected);
		return;
	}

	if (cid == BT_L2CAP_CID_LE_SIG) {
		(void)bt_atomic_inc(&sig_pdus);
		osi_sem_give(&sig_sent_sem);
	} else if (cid != BT_L2CAP_CID_ATT) {
		(void)bt_atomic_inc(&acl_unexpected);
	} else if (buf->data[0] == BT_ATT_OP_NOTIFY) {
		(void)bt_atomic_inc(&notifications);
	} else if (buf->data[0] == BT_ATT_OP_INDICATE) {
		(void)bt_atomic_inc(&indications);
	} else {
		(void)bt_atomic_inc(&acl_unexpected);
	}
}

static void completed_handler(struct bt_work *work)
{
	const struct bt_hci_transport *dev = &test_driver;
	struct bt_hci_evt_num_completed_packets *evt;
	struct bt_hci_handle_count *hc;
	struct net_buf *buf;
	bt_atomic_val_t count;

	ARG_UNUSED(work);

	count = bt_atomic_set(&acl_pending, 0);
	if (count == 0 || bt_atomic_get(&link_up) == 0) {
		return;
	}

	buf = bt_buf_get_evt(BT_HCI_EVT_NUM_COMPLETED_PACKETS, false, BT_FOREVER);
	evt_create(buf, BT_HCI_EVT_NUM_COMPLETED_PACKETS, sizeof(*evt) + sizeof(*hc));
	evt = net_buf_add(buf, sizeof(*evt));
	evt->num_handles = 1U;
	hc = net_buf_add(buf, sizeof(*hc));
	hc->handle = sys_cpu_to_le16(TEST_CONN_HANDLE);
	hc->count = sys_cpu_to_le16((uint16_t)count);
	bt_hci_recv(dev, buf);
}

static BT_WORK_DEFINE(completed_work, completed_handler);

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

		if (bt_atomic_cas(&acl_hold, 1, 0)) {
			acl_held = net_buf_ref(buf);
			osi_sem_give(&acl_held_sem);
		}

		/* As a transport that waits for the controller to take the
		 * packet. The TX processor holds the host lock meanwhile.
		 */
		if (bt_atomic_cas(&send_block, 1, 0)) {
			osi_sem_give(&send_blocked_sem);
			(void)osi_sem_take(&send_release_sem, BT_FOREVER);
		}

		if (bt_atomic_get(&link_up) != 0) {
			(void)bt_atomic_inc(&acl_pending);
			(void)bt_work_submit(&completed_work);
		}
	} else {
		(void)bt_atomic_inc(&acl_unexpected);
	}

	net_buf_unref(buf);

	return 0;
}

static const struct bt_hci_driver_api driver_api = {
	.open = driver_open,
	.send = driver_send,
};

static os_mutex_t observed_pool_lock;
static struct bt_hci_driver_data test_driver_data;
static const struct bt_hci_transport test_driver = {
 .name = "upstream-test", .bus = 0,
 .api = &driver_api, .data = &test_driver_data,
};
static void test_transport_init(void)
{
 static bool observer_initialized;
 if (!observer_initialized) {
  bt_test_require(osi_mutex_init(&observed_pool_lock, OS_MUTEX_NORMAL));
  observer_initialized = true;
 }
 bt_test_require(bt_stack_init());
 bt_dev.hci = &test_driver;
}


static os_sem_t connected_sem;
static os_sem_t disconnected_sem;
static os_sem_t recycled_sem;
static struct bt_conn *test_conn;

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0U) {
		return;
	}

	test_conn = bt_conn_ref(conn);
	osi_sem_give(&connected_sem);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARGS_UNUSED(conn, reason);

	osi_sem_give(&disconnected_sem);
}

static void recycled(void)
{
	osi_sem_give(&recycled_sem);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

static void evt_send(uint8_t evt, const void *params, uint8_t len)
{
	struct net_buf *buf;

	buf = bt_buf_get_evt(evt, false, BT_FOREVER);
	evt_create(buf, evt, len);
	net_buf_add_mem(buf, params, len);
	bt_hci_recv(&test_driver, buf);
}

static void connect_as_peripheral(void)
{
	const bt_addr_le_t peer = {
		.type = BT_ADDR_LE_RANDOM,
		.a.val = { 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0xc6U },
	};
	struct {
		struct bt_hci_evt_le_meta_event meta;
		struct bt_hci_evt_le_conn_complete cc;
	} __packed evt;

	assert_int_equal(bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, NULL, 0U, NULL, 0U), 0);

	(void)memset(&evt, 0, sizeof(evt));
	evt.meta.subevent = BT_HCI_EVT_LE_CONN_COMPLETE;
	evt.cc.status = BT_HCI_ERR_SUCCESS;
	evt.cc.handle = sys_cpu_to_le16(TEST_CONN_HANDLE);
	evt.cc.role = BT_HCI_ROLE_PERIPHERAL;
	bt_addr_le_copy(&evt.cc.peer_addr, &peer);
	evt.cc.interval = sys_cpu_to_le16(BT_GAP_INIT_CONN_INT_MIN);
	evt.cc.supv_timeout = sys_cpu_to_le16(BT_GAP_MS_TO_CONN_TIMEOUT(4000U));

	(void)bt_atomic_set(&link_up, 1);
	evt_send(BT_HCI_EVT_LE_META_EVENT, &evt, sizeof(evt));

	assert_int_equal(osi_sem_take(&connected_sem, BT_SECONDS(1)), 0);
}

/* The remote ends the connection. The controller drops what it holds for it. */
static void disconn_complete_send(void)
{
	const struct bt_hci_evt_disconn_complete evt = {
		.status = BT_HCI_ERR_SUCCESS,
		.handle = sys_cpu_to_le16(TEST_CONN_HANDLE),
		.reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN,
	};

	(void)bt_atomic_set(&link_up, 0);
	evt_send(BT_HCI_EVT_DISCONN_COMPLETE, &evt, sizeof(evt));
}

/* The connection is gone and released */
static int disconnected_wait(void)
{
	int err;

	err = osi_sem_take(&disconnected_sem, BT_SECONDS(1));
	if (err != 0) {
		return err;
	}

	bt_conn_unref(test_conn);
	test_conn = NULL;

	return osi_sem_take(&recycled_sem, BT_SECONDS(1));
}

static struct net_buf_pool *observed_pools[32];
static size_t observed_pool_count;
struct net_buf *__real_net_buf_alloc_fixed(struct net_buf_pool *, bt_timeout_t);
struct net_buf *__wrap_net_buf_alloc_fixed(struct net_buf_pool *pool, bt_timeout_t timeout)
{
 bt_test_require(osi_mutex_lock(&observed_pool_lock, OS_TIMEOUT_FOREVER));
 size_t index;
 for (index = 0; index < observed_pool_count; index++) {
  if (observed_pools[index] == pool) { break; }
 }
 if (index == observed_pool_count) {
  if (observed_pool_count == ARRAY_SIZE(observed_pools)) { abort(); }
  observed_pools[observed_pool_count++] = pool;
 }
 bt_test_require(osi_mutex_unlock(&observed_pool_lock));
 return __real_net_buf_alloc_fixed(pool, timeout);
}

static bool pools_full(void)
{
 bool full = true;
 bt_test_require(osi_mutex_lock(&observed_pool_lock, OS_TIMEOUT_FOREVER));
 for (size_t i = 0; i < observed_pool_count; i++) {
  struct net_buf_pool *pool = observed_pools[i];
  if (bt_atomic_get(&pool->avail_count) != pool->buf_count) { full = false; break; }
 }
 bt_test_require(osi_mutex_unlock(&observed_pool_lock));
 return full;
}

/* No buffer of any pool is in use. Buffers are freed on other threads too,
 * which are given the time to do so.
 */
static void pools_full_expect(void)
{
	for (unsigned int i = 0U; i < 100U && !pools_full(); i++) {
		osi_sleep_ms(BT_MSEC(10));
	}

	assert_true(pools_full());
}

static int notify(void)
{
	static const uint8_t value = 0x5aU;

	return bt_gatt_notify(test_conn, &test_svc.attrs[2], &value, sizeof(value));
}

/* A Security Request, which a peripheral sends over L2CAP without ATT */
static int security_request(void)
{
	return bt_conn_set_security(test_conn, BT_SECURITY_L2);
}

/* What the application thread sends, and what that returned */
static int (*app_send)(void);
static bt_atomic_t app_send_done;
static int app_send_err;

/* An application thread, which the Host's threads preempt */
static void app_entry(void *p1)
{
	ARG_UNUSED(p1);

	app_send_err = app_send();
	(void)bt_atomic_set(&app_send_done, 1);
}

static bt_atomic_t indicate_done;
static os_sem_t indication_done_sem;

static void indicated(struct bt_conn *conn, struct bt_gatt_indicate_params *params, uint8_t err)
{
	ARGS_UNUSED(conn, params, err);

	(void)bt_atomic_set(&indicate_done, 1);
	osi_sem_give(&indication_done_sem);
}

/* An indication, which the peer does not confirm */
static int indicate(void)
{
	static struct bt_gatt_indicate_params params;
	static const uint8_t value = 0xa5U;

	params = (struct bt_gatt_indicate_params){
		.attr = &test_svc.attrs[2],
		.func = indicated,
		.data = &value,
		.len = sizeof(value),
	};

	return bt_gatt_indicate(test_conn, &params);
}

static bt_atomic_t notify_sent_count;

static void notify_sent(struct bt_conn *conn, void *user_data)
{
	ARGS_UNUSED(conn, user_data);

	(void)bt_atomic_inc(&notify_sent_count);
}

/* A notification whose completion the application is told about */
static int notify_reported(void)
{
	static const uint8_t value = 0x5aU;
	struct bt_gatt_notify_params params = {
		.attr = &test_svc.attrs[2],
		.data = &value,
		.len = sizeof(value),
		.func = notify_sent,
	};

	return bt_gatt_notify_cb(test_conn, &params);
}

/* A data packet from the peer with an L2CAP PDU of len bytes, which the caller
 * adds
 */
static struct net_buf *l2cap_pdu_create(uint16_t cid, uint16_t len)
{
	struct bt_hci_acl_hdr *acl;
	struct bt_l2cap_hdr *hdr;
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_FOREVER);
	acl = net_buf_add(buf, sizeof(*acl));
	acl->handle = sys_cpu_to_le16(bt_acl_handle_pack(TEST_CONN_HANDLE, BT_ACL_START));
	acl->len = sys_cpu_to_le16(sizeof(*hdr) + len);
	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->len = sys_cpu_to_le16(len);
	hdr->cid = sys_cpu_to_le16(cid);

	return buf;
}

/* A Read Request from the peer */
static void read_req_recv(uint16_t handle)
{
	struct net_buf *buf;

	buf = l2cap_pdu_create(BT_L2CAP_CID_ATT,
			       sizeof(struct bt_att_hdr) + sizeof(struct bt_att_read_req));
	net_buf_add_u8(buf, BT_ATT_OP_READ_REQ);
	net_buf_add_le16(buf, handle);

	bt_hci_recv(&test_driver, buf);
}

/* A signalling command from the peer */
static void sig_recv(uint8_t code, uint8_t ident, const void *data, uint16_t len)
{
	struct bt_l2cap_sig_hdr *hdr;
	struct net_buf *buf;

	buf = l2cap_pdu_create(BT_L2CAP_CID_LE_SIG, sizeof(*hdr) + len);
	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->code = code;
	hdr->ident = ident;
	hdr->len = sys_cpu_to_le16(len);
	net_buf_add_mem(buf, data, len);

	bt_hci_recv(&test_driver, buf);
}

/* A credit-based channel, which the peer opens */
#define TEST_PSM 0x0080U
#define PEER_CID 0x0040U
#define PEER_MTU 23U

/* An SDU of one byte, which the application sends on it */
NET_BUF_POOL_FIXED_DEFINE(sdu_pool, 1, BT_L2CAP_SDU_CHAN_SEND_RESERVE + 1U,
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static struct bt_l2cap_le_chan test_chan;
static os_sem_t chan_connected_sem;

/* The disconnected callback of the channel blocks until the test lets it go */
static bt_atomic_t chan_disconnected_block;
static os_sem_t chan_disconnected_sem;
static os_sem_t chan_release_sem;

static int chan_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	ARGS_UNUSED(chan, buf);

	return 0;
}

static void chan_connected(struct bt_l2cap_chan *chan)
{
	ARG_UNUSED(chan);

	osi_sem_give(&chan_connected_sem);
}

/* As an application that waits in the callback, which runs on bt_workq */
static void chan_disconnected(struct bt_l2cap_chan *chan)
{
	ARG_UNUSED(chan);

	if (bt_atomic_cas(&chan_disconnected_block, 1, 0)) {
		osi_sem_give(&chan_disconnected_sem);
		(void)osi_sem_take(&chan_release_sem, BT_FOREVER);
	}
}

static int chan_accept(struct bt_conn *conn, struct bt_l2cap_server *server,
		       struct bt_l2cap_chan **chan)
{
	static const struct bt_l2cap_chan_ops ops = {
		.connected = chan_connected,
		.disconnected = chan_disconnected,
		.recv = chan_recv,
	};

	ARGS_UNUSED(conn, server);

	test_chan = (struct bt_l2cap_le_chan){ .chan.ops = &ops };
	*chan = &test_chan.chan;

	return 0;
}

/* bt_workq runs nothing else from blocker_handler() until the test lets it */
static os_sem_t blocker_running_sem;
static os_sem_t blocker_sem;

static void blocker_handler(struct bt_work *work)
{
	ARG_UNUSED(work);

	osi_sem_give(&blocker_running_sem);
	(void)osi_sem_take(&blocker_sem, BT_FOREVER);
}

static os_sem_t system_blocker_running_sem;
static os_sem_t system_blocker_sem;

/** @brief Hold deferred TX completion while the Host finishes timeout handling. */
static void system_blocker_handler(struct bt_work *work)
{
	ARG_UNUSED(work);
	osi_sem_give(&system_blocker_running_sem);
	osi_sem_take(&system_blocker_sem, BT_FOREVER);
}

static void *setup(void)
{
	assert_int_equal(bt_gatt_service_static_register(&test_svc), 0);
	static struct bt_l2cap_server server = {
		.psm = TEST_PSM,
		.accept = chan_accept,
	};

	assert_int_equal(bt_enable(NULL), 0);
	assert_int_equal(bt_l2cap_server_register(&server), 0);

	return NULL;
}

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	(void)bt_atomic_clear(&notifications);
	(void)bt_atomic_clear(&indications);
	(void)bt_atomic_clear(&sig_pdus);
	(void)bt_atomic_clear(&acl_unexpected);
	(void)bt_atomic_clear(&app_send_done);
	(void)bt_atomic_clear(&indicate_done);
	(void)bt_atomic_clear(&notify_sent_count);
}



/* An application thread sends while the connection is lost, and waits for the
 * host lock meanwhile. Returns what the send returned. No buffer stays in use.
 */
static int send_during_disconnection(int (*send)(void))
{
	static os_thread_t app_thread;

	pools_full_expect();
	connect_as_peripheral();

	/* The TX processor stays in the driver's send() of a notification */
	(void)bt_atomic_set(&send_block, 1);
	assert_int_equal(notify(), 0);
	assert_int_equal(osi_sem_take(&send_blocked_sem, BT_SECONDS(1)), 0);

	/* The application thread waits for the TX processor */
	app_send = send;
	bt_test_require(osi_thread_create(&app_thread, app_entry, NULL, "test-app", 0, 0));
	assert_int_equal(bt_atomic_get(&app_send_done), 0);

	/* The connection is lost meanwhile. Its handling waits for the TX
	 * processor as well.
	 */
	disconn_complete_send();
	do { unsigned int count; assert_int_equal(osi_sem_count_get(&disconnected_sem, &count), 0); assert_int_equal(count, 0U); } while (0);

	osi_sem_give(&send_release_sem);

	assert_int_equal(osi_thread_join(&app_thread), 0);
	assert_int_equal(disconnected_wait(), 0);
	assert_int_equal(bt_atomic_get(&notifications), 1);
	assert_int_equal(bt_atomic_get(&acl_unexpected), 0);
	pools_full_expect();

	return app_send_err;
}

/* A notification is refused */
static void host_tx_disconnect__test_notify_during_disconnection(void **cm_state)
{
	(void)cm_state;
	int err = send_during_disconnection(notify);

	assert_int_equal(err, -ENOTCONN);
}

/* A Security Request is refused */
static void host_tx_disconnect__test_security_request_during_disconnection(void **cm_state)
{
	(void)cm_state;
	int err = send_during_disconnection(security_request);

	assert_int_equal(err, -ENOTCONN);
}

/* The completion of a notification waits for the host lock while ATT times
 * out, on a connection that is being lost, with the response to a request of
 * the peer left to send. The completion is dropped, and no buffer stays in use.
 */
static void host_tx_disconnect__test_sent_during_att_timeout(void **cm_state)
{
	(void)cm_state;
	static BT_WORK_DEFINE(blocker_work, blocker_handler);
	static BT_WORK_DEFINE(system_blocker_work, system_blocker_handler);

	pools_full_expect();
	connect_as_peripheral();

	/* ATT times out the indication 30 seconds after sending it */
	assert_int_equal(indicate(), 0);
	osi_sleep_ms(BT_MSEC(29900));

	/* The controller keeps the buffer of a notification */
	(void)bt_atomic_set(&acl_hold, 1);
	assert_int_equal(notify_reported(), 0);
	assert_int_equal(osi_sem_take(&acl_held_sem, BT_SECONDS(1)), 0);
	assert_non_null(acl_held);

	/* The test holds the host lock, as the TX processor does in a driver's
	 * send() that blocks. A Read Request from the peer waits for it.
	 */
	bt_dev_lock();
	read_req_recv(bt_gatt_attr_get_handle(&test_svc.attrs[0]));

	/* bt_workq stops after the request, and ATT times out meanwhile */
	assert_true(bt_host_work_submit(&blocker_work) >= 0);
	osi_sleep_ms(BT_MSEC(200));

	/* The connection is lost, so the response is left to send */
	disconn_complete_send();
	bt_dev_unlock();
	assert_int_equal(osi_sem_take(&blocker_running_sem, BT_SECONDS(1)), 0);
	assert_true(bt_work_submit(&system_blocker_work) >= 0);
	assert_int_equal(osi_sem_take(&system_blocker_running_sem, BT_SECONDS(1)), 0);

	/* The completion of the notification waits for the host lock, while
	 * bt_workq handles the ATT timeout
	 */
	bt_dev_lock();
	net_buf_drop(&acl_held);
	assert_int_equal(bt_atomic_get(&notify_sent_count), 0);
	osi_sem_give(&blocker_sem);
	bt_dev_unlock();
	/* Wait for ATT to finish the indication before permitting its TX callback. */
	assert_int_equal(osi_sem_take(&indication_done_sem, BT_SECONDS(1)), 0);
	osi_sem_give(&system_blocker_sem);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_int_equal(bt_work_queue_drain(&bt_sys_work_q, false), 0);

	assert_int_equal(disconnected_wait(), 0);
	assert_int_equal(bt_atomic_get(&indicate_done), 1);
	assert_int_equal(bt_atomic_get(&notify_sent_count), 0);
	assert_int_equal(bt_atomic_get(&indications), 1);
	assert_int_equal(bt_atomic_get(&notifications), 1);
	assert_int_equal(bt_atomic_get(&acl_unexpected), 0);
	pools_full_expect();
}

/* The peer closes a credit-based channel while the application waits in the
 * disconnected callback of the channel, and an SDU is sent on the channel
 * meanwhile. The SDU is refused, and the channel is not left ready to send.
 */
static void host_tx_disconnect__test_sdu_during_chan_disconnection(void **cm_state)
{
	(void)cm_state;
	const struct bt_l2cap_le_conn_req conn_req = {
		.psm = sys_cpu_to_le16(TEST_PSM),
		.scid = sys_cpu_to_le16(PEER_CID),
		.mtu = sys_cpu_to_le16(PEER_MTU),
		.mps = sys_cpu_to_le16(PEER_MTU),
		.credits = sys_cpu_to_le16(1U),
	};
	struct bt_l2cap_disconn_req disconn_req;
	struct net_buf *sdu;
	int err;

	pools_full_expect();
	connect_as_peripheral();

	sig_recv(BT_L2CAP_LE_CONN_REQ, 0x01U, &conn_req, sizeof(conn_req));
	assert_int_equal(osi_sem_take(&chan_connected_sem, BT_SECONDS(1)), 0);

	disconn_req = (struct bt_l2cap_disconn_req){
		.dcid = sys_cpu_to_le16(test_chan.rx.cid),
		.scid = sys_cpu_to_le16(PEER_CID),
	};
	(void)bt_atomic_set(&chan_disconnected_block, 1);
	sig_recv(BT_L2CAP_DISCONN_REQ, 0x02U, &disconn_req, sizeof(disconn_req));
	assert_int_equal(osi_sem_take(&chan_disconnected_sem, BT_SECONDS(1)), 0);

	sdu = net_buf_alloc(&sdu_pool, BT_NO_WAIT);
	assert_non_null(sdu);
	net_buf_reserve(sdu, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
	net_buf_add_u8(sdu, 0x5aU);

	err = bt_l2cap_chan_send(&test_chan.chan, sdu);
	assert_int_equal(err, -ENOTCONN);
	net_buf_unref(sdu);
	assert_true(sys_slist_is_empty(&test_conn->l2cap_data_ready));

	osi_sem_give(&chan_release_sem);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_int_equal(osi_sem_take(&sig_sent_sem, BT_SECONDS(1)), 0);
	assert_int_equal(osi_sem_take(&sig_sent_sem, BT_SECONDS(1)), 0);

	disconn_complete_send();
	assert_int_equal(disconnected_wait(), 0);
	assert_int_equal(bt_atomic_get(&sig_pdus), 2);
	assert_int_equal(bt_atomic_get(&acl_unexpected), 0);
	pools_full_expect();
}

static void storage_init_99525333d6(void)
{
	bt_test_require(osi_sem_init(&sig_sent_sem, 0, 2));
	bt_test_require(osi_sem_init(&acl_held_sem, 0, 1));
	bt_test_require(osi_sem_init(&send_blocked_sem, 0, 1));
	bt_test_require(osi_sem_init(&send_release_sem, 0, 1));
	bt_test_require(osi_sem_init(&connected_sem, 0, 1));
	bt_test_require(osi_sem_init(&disconnected_sem, 0, 1));
	bt_test_require(osi_sem_init(&recycled_sem, 0, 1));
	net_buf_pool_init(&sdu_pool);
	bt_test_require(osi_sem_init(&chan_connected_sem, 0, 1));
	bt_test_require(osi_sem_init(&chan_disconnected_sem, 0, 1));
	bt_test_require(osi_sem_init(&chan_release_sem, 0, 1));
	bt_test_require(osi_sem_init(&indication_done_sem, 0, 1));
	bt_test_require(osi_sem_init(&system_blocker_running_sem, 0, 1));
	bt_test_require(osi_sem_init(&system_blocker_sem, 0, 1));
	bt_test_require(osi_sem_init(&blocker_running_sem, 0, 1));
	bt_test_require(osi_sem_init(&blocker_sem, 0, 1));
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&sdu_pool);
}

static void migrated_callbacks_init(void)
{
 static bool initialized;
 if (initialized) { return; }
 bt_test_require(bt_conn_cb_register(&bt_conn_cb_conn_callbacks));
 initialized = true;
}

int bt_test_host_tx_disconnect_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	migrated_pools_init();
	test_transport_init();
	storage_init_99525333d6();
	*state = setup();
	return 0;
}
static int bt_test_host_tx_disconnect_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}


static const struct CMUnitTest test_cases_host_tx_disconnect[] = {
	cmocka_unit_test_setup_teardown(host_tx_disconnect__test_notify_during_disconnection, bt_test_host_tx_disconnect_before, NULL),
	cmocka_unit_test_setup_teardown(host_tx_disconnect__test_security_request_during_disconnection, bt_test_host_tx_disconnect_before, NULL),
	cmocka_unit_test_setup_teardown(host_tx_disconnect__test_sent_during_att_timeout, bt_test_host_tx_disconnect_before, NULL),
	cmocka_unit_test_setup_teardown(host_tx_disconnect__test_sdu_during_chan_disconnection, bt_test_host_tx_disconnect_before, NULL),
};
BT_TEST_SUITE(host_tx_disconnect, test_cases_host_tx_disconnect, bt_test_host_tx_disconnect_setup, NULL);
