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
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include <embt/base/log.h>
LOG_MODULE_REGISTER(bt_adv_dir_timeout_test, CONFIG_BT_LOG_LEVEL);


/* Length of the command complete parameters used for commands without a dedicated handler.
 * The parameters are zeroed, and the size is large enough to hold the return parameters of any
 * command that the Host may send.
 */
#define UNHANDLED_CMD_RP_SIZE 64U

/* The test Controller reports the timeout of high duty cycle directed advertising with a
 * connection complete event. Without this quirk an LE Advertising Set Terminated event follows.
 */
#define CONN_COMPLETE_ONLY                                                                         \
	((test_driver.quirks & BT_HCI_QUIRK_NO_ADV_TERM_ON_DIR_TIMEOUT) != 0)

/* Advertising handle of the most recent LE Set Extended Advertising Parameters command. */
static uint8_t last_adv_handle;

/* Error of the most recent connected callback. */
static uint8_t conn_err;

static os_sem_t connected_sem;
static os_sem_t sent_sem;

/* Command handler structure for cmd_handle(). */
struct cmd_handler {
	uint16_t opcode; /* HCI command opcode */
	uint8_t len;     /* HCI command response length */
	void (*handler)(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode);
};

/* Add event to net_buf. */
static void evt_create(struct net_buf *buf, uint8_t evt, uint8_t len)
{
	struct bt_hci_evt_hdr *hdr;

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = evt;
	hdr->len = len;
}

/* Create a command complete event. */
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

/* Generic command complete with success status. */
static void generic_success(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode)
{
	struct bt_hci_evt_cc_status *ccst;

	ARG_UNUSED(buf);

	ccst = cmd_complete(evt, len, opcode);

	/* Fill any event parameters with zero */
	(void)memset(ccst, 0, len);

	ccst->status = BT_HCI_ERR_SUCCESS;
}

static void read_local_features(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				uint16_t opcode)
{
	struct bt_hci_rp_read_local_features *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->features, 0, sizeof(rp->features));
	/* LE supported, and BR/EDR not supported */
	rp->features[4] = BIT(5) | BIT(6);
}

static void read_supported_commands(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				    uint16_t opcode)
{
	struct bt_hci_rp_read_supported_commands *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->commands, 0xFF, sizeof(rp->commands));
}

static void read_bd_addr(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_read_bd_addr *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(&rp->bdaddr, 0x11, sizeof(rp->bdaddr));
}

static void le_read_local_features(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				   uint16_t opcode)
{
	struct bt_hci_rp_le_read_local_features *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->features, 0, sizeof(rp->features));
	/* Only report support for LE Extended Advertising */
	rp->features[BT_LE_FEAT_BIT_EXT_ADV / 8] = BIT(BT_LE_FEAT_BIT_EXT_ADV % 8);
}

static void le_read_buffer_size(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				uint16_t opcode)
{
	struct bt_hci_rp_le_read_buffer_size *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->le_max_len = sys_cpu_to_le16(27U);
	rp->le_max_num = 1U;
}

static void le_read_supp_states(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				uint16_t opcode)
{
	struct bt_hci_rp_le_read_supp_states *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	(void)memset(rp->le_states, 0xFF, sizeof(rp->le_states));
}

static void le_read_max_adv_data_len(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				     uint16_t opcode)
{
	struct bt_hci_rp_le_read_max_adv_data_len *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->max_adv_data_len = sys_cpu_to_le16(BT_GAP_ADV_MAX_EXT_ADV_DATA_LEN);
}

/* Handler for BT_HCI_OP_LE_SET_EXT_ADV_PARAM. It stores the advertising handle, which the
 * LE Advertising Set Terminated event of the test Controller has to carry.
 */
static void le_set_ext_adv_param(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				 uint16_t opcode)
{
	const struct bt_hci_cp_le_set_ext_adv_param *cp = (void *)buf->data;
	struct bt_hci_rp_le_set_ext_adv_param *rp;

	ARG_UNUSED(len);

	assert_true(buf->len >= sizeof(*cp));

	last_adv_handle = cp->handle;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->tx_power = 0;
}

/* Handlers for the commands where the Host requires specific return parameters. Any other
 * command is accepted by cmd_handle() with zeroed return parameters.
 */
static const struct cmd_handler cmds[] = {
	{
		BT_HCI_OP_READ_LOCAL_FEATURES,
		sizeof(struct bt_hci_rp_read_local_features),
		read_local_features,
	},
	{
		BT_HCI_OP_READ_SUPPORTED_COMMANDS,
		sizeof(struct bt_hci_rp_read_supported_commands),
		read_supported_commands,
	},
	{
		BT_HCI_OP_READ_BD_ADDR,
		sizeof(struct bt_hci_rp_read_bd_addr),
		read_bd_addr,
	},
	{
		BT_HCI_OP_LE_READ_LOCAL_FEATURES,
		sizeof(struct bt_hci_rp_le_read_local_features),
		le_read_local_features,
	},
	{
		BT_HCI_OP_LE_READ_BUFFER_SIZE,
		sizeof(struct bt_hci_rp_le_read_buffer_size),
		le_read_buffer_size,
	},
	{
		BT_HCI_OP_LE_READ_SUPP_STATES,
		sizeof(struct bt_hci_rp_le_read_supp_states),
		le_read_supp_states,
	},
	{
		BT_HCI_OP_LE_READ_MAX_ADV_DATA_LEN,
		sizeof(struct bt_hci_rp_le_read_max_adv_data_len),
		le_read_max_adv_data_len,
	},
	{
		BT_HCI_OP_LE_SET_EXT_ADV_PARAM,
		sizeof(struct bt_hci_rp_le_set_ext_adv_param),
		le_set_ext_adv_param,
	},
};

/* Lookup the command opcode and invoke the handler. */
static void cmd_handle(const struct bt_hci_transport *dev, struct net_buf *cmd)
{
	struct net_buf *evt = NULL;
	struct bt_hci_cmd_hdr *chdr;
	uint16_t opcode;

	chdr = net_buf_pull_mem(cmd, sizeof(*chdr));
	opcode = sys_le16_to_cpu(chdr->opcode);

	ARRAY_FOR_EACH_PTR(cmds, handler) {
		if (handler->opcode == opcode) {
			handler->handler(cmd, &evt, handler->len, opcode);
			bt_hci_recv(dev, evt);

			return;
		}
	}

	LOG_DBG("Unhandled opcode 0x%04X", opcode);
	generic_success(cmd, &evt, UNHANDLED_CMD_RP_SIZE, opcode);
	bt_hci_recv(dev, evt);
}

static int driver_open(const struct bt_hci_transport *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int driver_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	uint8_t type = net_buf_pull_u8(buf);

	assert_int_equal(type, BT_HCI_H4_CMD);

	cmd_handle(dev, buf);

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
 .quirks = IS_ENABLED(CONFIG_TEST_NO_ADV_TERM_ON_DIR_TIMEOUT) ?
           BT_HCI_QUIRK_NO_ADV_TERM_ON_DIR_TIMEOUT : 0,
 .api = &driver_api, .data = &test_driver_data,
};
static void test_transport_init(void)
{
 bt_test_require(bt_stack_init());
 bt_dev.hci = &test_driver;
}


static void *le_meta_event(struct net_buf **buf, uint8_t subevent, uint8_t len)
{
	struct bt_hci_evt_le_meta_event *meta;

	*buf = bt_buf_get_evt(BT_HCI_EVT_LE_META_EVENT, false, BT_FOREVER);
	evt_create(*buf, BT_HCI_EVT_LE_META_EVENT, sizeof(*meta) + len);
	meta = net_buf_add(*buf, sizeof(*meta));
	meta->subevent = subevent;

	return net_buf_add(*buf, len);
}

/* Let the test Controller report that no connection was created before the duration expired. */
static void conn_complete_timeout(void)
{
	struct bt_hci_evt_le_enh_conn_complete *cc;
	struct net_buf *buf;

	cc = le_meta_event(&buf, BT_HCI_EVT_LE_ENH_CONN_COMPLETE, sizeof(*cc));
	(void)memset(cc, 0, sizeof(*cc));
	cc->status = BT_HCI_ERR_ADV_TIMEOUT;
	cc->role = BT_HCI_ROLE_PERIPHERAL;
	bt_hci_recv(&test_driver, buf);
}

/* Let the test Controller report the timeout with the events that it sends, and check that the
 * Host ends the advertising when the event that it has to act on arrives.
 */
static void adv_timeout(void)
{
	int err;

	conn_complete_timeout();

	if (!CONN_COMPLETE_ONLY) {
		struct bt_hci_evt_le_adv_set_terminated *term;
		struct net_buf *buf;

		err = osi_sem_take(&connected_sem, BT_MSEC(100));
		assert_int_equal(err, -ETIMEDOUT);

		term = le_meta_event(&buf, BT_HCI_EVT_LE_ADV_SET_TERMINATED, sizeof(*term));
		(void)memset(term, 0, sizeof(*term));
		term->status = BT_HCI_ERR_ADV_TIMEOUT;
		term->adv_handle = last_adv_handle;
		bt_hci_recv(&test_driver, buf);
	}

	err = osi_sem_take(&connected_sem, BT_SECONDS(1));
	assert_int_equal(err, 0);
	assert_int_equal(conn_err, BT_HCI_ERR_ADV_TIMEOUT);
	assert_int_equal(bt_host_work_drain(), 0);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	ARG_UNUSED(conn);

	conn_err = err;
	osi_sem_give(&connected_sem);
}

static void adv_sent(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(info);

	osi_sem_give(&sent_sem);
}

static const struct bt_le_ext_adv_cb adv_cb = {
	.sent = adv_sent,
};

static void *adv_dir_timeout_setup(void)
{
	static struct bt_conn_cb conn_cb = {
		.connected = connected,
	};
	int err;

	err = bt_enable(NULL);
	assert_int_equal(err, 0);

	err = bt_conn_cb_register(&conn_cb);
	assert_int_equal(err, 0);

	return NULL;
}



static const bt_addr_le_t peer = {
	.type = BT_ADDR_LE_RANDOM,
	.a = { { 0x01, 0x02, 0x03, 0x04, 0x05, 0xc6 } },
};

static const bt_addr_le_t other_peer = {
	.type = BT_ADDR_LE_RANDOM,
	.a = { { 0x06, 0x05, 0x04, 0x03, 0x02, 0xc1 } },
};

static void bt_adv_dir_timeout__test_ext_adv_start(void **cm_state)
{
	(void)cm_state;
	struct bt_le_ext_adv *adv;
	int err;

	err = bt_le_ext_adv_create(BT_LE_ADV_CONN_DIR(&peer), &adv_cb, &adv);
	assert_int_equal(err, 0);

	/* The second round fails to start if the first timeout left the set enabled */
	for (size_t i = 0U; i < 2U; i++) {
		struct bt_le_ext_adv_info info;

		err = bt_le_ext_adv_start(
			adv, BT_LE_EXT_ADV_START_PARAM(BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT, 0));
		assert_int_equal(err, 0);

		adv_timeout();

		err = osi_sem_take(&sent_sem, BT_SECONDS(1));
		assert_int_equal(err, 0);

		err = bt_le_ext_adv_get_info(adv, &info);
		assert_int_equal(err, 0);
		assert_int_equal(info.ext_adv_state, BT_LE_EXT_ADV_STATE_DISABLED);
	}

	err = bt_le_ext_adv_delete(adv);
	assert_int_equal(err, 0);
}

static void bt_adv_dir_timeout__test_adv_start(void **cm_state)
{
	(void)cm_state;
	/* The second round fails to start if the first timeout left the advertiser enabled */
	for (size_t i = 0U; i < 2U; i++) {
		int err;

		err = bt_le_adv_start(BT_LE_ADV_CONN_DIR(&peer), NULL, 0, NULL, 0);
		assert_int_equal(err, 0);

		adv_timeout();
	}
}

/* A connection complete event for the timeout that arrives after the application stopped the
 * set has no effect, on that set or on its next start.
 */
static void bt_adv_dir_timeout__test_ext_adv_stopped(void **cm_state)
{
	(void)cm_state;
	struct bt_le_ext_adv_info info;
	struct bt_le_ext_adv *adv;
	int err;

	err = bt_le_ext_adv_create(BT_LE_ADV_CONN_DIR(&peer), &adv_cb, &adv);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_start(
		adv, BT_LE_EXT_ADV_START_PARAM(BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT, 0));
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_stop(adv);
	assert_int_equal(err, 0);

	conn_complete_timeout();

	err = osi_sem_take(&connected_sem, BT_MSEC(100));
	assert_int_equal(err, -ETIMEDOUT);
	err = osi_sem_take(&sent_sem, BT_NO_WAIT);
	assert_int_equal(err, -EAGAIN);

	err = bt_le_ext_adv_start(
		adv, BT_LE_EXT_ADV_START_PARAM(BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT, 0));
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_get_info(adv, &info);
	assert_int_equal(err, 0);
	assert_int_equal(info.ext_adv_state, BT_LE_EXT_ADV_STATE_ENABLED);

	err = bt_le_ext_adv_stop(adv);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_delete(adv);
	assert_int_equal(err, 0);
}

/* The timeout of the high duty cycle set must leave a low duty cycle directed set enabled. */
static void bt_adv_dir_timeout__test_ext_adv_low_duty_unaffected(void **cm_state)
{
	(void)cm_state;
	struct bt_le_ext_adv *low_duty;
	struct bt_le_ext_adv_info info;
	struct bt_le_ext_adv *adv;
	int err;

	if (!CONN_COMPLETE_ONLY || CONFIG_BT_EXT_ADV_MAX_ADV_SET < 2) {
		skip();
	}

	err = bt_le_ext_adv_create(BT_LE_ADV_CONN_DIR_LOW_DUTY(&other_peer), NULL, &low_duty);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_create(BT_LE_ADV_CONN_DIR(&peer), &adv_cb, &adv);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_start(low_duty, BT_LE_EXT_ADV_START_DEFAULT);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_start(
		adv, BT_LE_EXT_ADV_START_PARAM(BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT, 0));
	assert_int_equal(err, 0);

	adv_timeout();

	err = osi_sem_take(&sent_sem, BT_SECONDS(1));
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_get_info(adv, &info);
	assert_int_equal(err, 0);
	assert_int_equal(info.ext_adv_state, BT_LE_EXT_ADV_STATE_DISABLED);

	err = bt_le_ext_adv_get_info(low_duty, &info);
	assert_int_equal(err, 0);
	assert_int_equal(info.ext_adv_state, BT_LE_EXT_ADV_STATE_ENABLED);

	err = bt_le_ext_adv_stop(low_duty);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_delete(low_duty);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_delete(adv);
	assert_int_equal(err, 0);
}

/* The connection complete event names no advertising set, so with the quirk only one high duty
 * cycle directed set can be enabled.
 */
static void bt_adv_dir_timeout__test_ext_adv_second_high_duty(void **cm_state)
{
	(void)cm_state;
	struct bt_le_ext_adv *second;
	struct bt_le_ext_adv *adv;
	int err;

	if (!CONN_COMPLETE_ONLY || CONFIG_BT_EXT_ADV_MAX_ADV_SET < 2) {
		skip();
	}

	err = bt_le_ext_adv_create(BT_LE_ADV_CONN_DIR(&peer), &adv_cb, &adv);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_start(
		adv, BT_LE_EXT_ADV_START_PARAM(BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT, 0));
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_create(BT_LE_ADV_CONN_DIR(&other_peer), NULL, &second);
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_start(
		second, BT_LE_EXT_ADV_START_PARAM(BT_GAP_ADV_HIGH_DUTY_CYCLE_MAX_TIMEOUT, 0));
	assert_int_equal(err, -EBUSY);

	err = bt_le_ext_adv_delete(second);
	assert_int_equal(err, 0);

	err = bt_le_adv_start(BT_LE_ADV_CONN_DIR(&other_peer), NULL, 0, NULL, 0);
	assert_int_equal(err, -EBUSY);

	adv_timeout();

	err = osi_sem_take(&sent_sem, BT_SECONDS(1));
	assert_int_equal(err, 0);

	err = bt_le_ext_adv_delete(adv);
	assert_int_equal(err, 0);
}

static void storage_init_8ae11f157a(void)
{
	bt_test_require(osi_sem_init(&connected_sem, 0, 1));
	bt_test_require(osi_sem_init(&sent_sem, 0, 1));
}

int bt_test_bt_adv_dir_timeout_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	test_transport_init();
	storage_init_8ae11f157a();
	*state = adv_dir_timeout_setup();
	return 0;
}



static const struct CMUnitTest test_cases_bt_adv_dir_timeout[] = {
	cmocka_unit_test(bt_adv_dir_timeout__test_ext_adv_start),
	cmocka_unit_test(bt_adv_dir_timeout__test_adv_start),
	cmocka_unit_test(bt_adv_dir_timeout__test_ext_adv_stopped),
	cmocka_unit_test(bt_adv_dir_timeout__test_ext_adv_low_duty_unaffected),
	cmocka_unit_test(bt_adv_dir_timeout__test_ext_adv_second_high_duty),
};
BT_TEST_SUITE(bt_adv_dir_timeout, test_cases_bt_adv_dir_timeout, bt_test_bt_adv_dir_timeout_setup, NULL);
