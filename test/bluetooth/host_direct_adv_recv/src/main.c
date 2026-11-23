#include "support/test_support.h"
static const struct bt_hci_transport test_driver;
#include "host/hci_core.h"
/*
 * Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests for the LE Directed Advertising Report handling in the host.
 *
 * The events are fed to the host through a fake HCI driver, so the LE event
 * mask, the meta event dispatch and the scanner state are all exercised, and
 * the reports are observed where an application would see them: in the scan
 * recv callback.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/gap.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/base/byteorder.h>

#define TEST_RSSI (-42)

/* Advertiser address carried by the crafted events. */
static const bt_addr_le_t test_adv_addr = {
	.type = BT_ADDR_LE_RANDOM,
	.a = {.val = {0x01, 0x02, 0x03, 0x04, 0x05, 0xc6}},
};

/* A resolvable private address, i.e. the two most significant bits are 0b01. */
static const bt_addr_le_t test_target_rpa = {
	.type = BT_ADDR_LE_RANDOM,
	.a = {.val = {0x9a, 0x3d, 0xe0, 0x55, 0xb1, 0x7c}},
};

/* LE features the fake Controller reports. Set before bt_enable(). */
static uint8_t test_le_features[8];

/* LE event mask the host asked the fake Controller for. */
static uint64_t captured_le_event_mask;

/* Scanning filter policy from the last scan parameter command. */
static uint8_t captured_filter_policy;

/* Captured state from the last scan recv callback invocation. */
struct captured_report {
	bt_addr_le_t addr;
	bt_addr_le_t direct_addr;
	bool has_direct_addr;
	uint8_t adv_type;
	uint16_t adv_props;
	int8_t rssi;
	uint8_t sid;
	uint16_t buf_len;
};

static struct captured_report last_report;
static unsigned int recv_call_count;

static void scan_recv_cb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	recv_call_count++;

	bt_addr_le_copy(&last_report.addr, info->addr);
	last_report.adv_type = info->adv_type;
	last_report.adv_props = info->adv_props;
	last_report.rssi = info->rssi;
	last_report.sid = info->sid;
	last_report.buf_len = buf->len;

	last_report.has_direct_addr = info->direct_addr != NULL;
	if (info->direct_addr != NULL) {
		bt_addr_le_copy(&last_report.direct_addr, info->direct_addr);
	}
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv_cb,
};

/*
 * Fake HCI driver
 */

struct cmd_handler {
	uint16_t opcode;
	uint8_t len;
	void (*handler)(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode);
};

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

static void generic_success(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode)
{
	struct bt_hci_evt_cc_status *ccst;

	ccst = cmd_complete(evt, len, opcode);
	(void)memset(ccst, 0, len);
	ccst->status = BT_HCI_ERR_SUCCESS;
}

static void read_local_features(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				uint16_t opcode)
{
	struct bt_hci_rp_read_local_features *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = 0x00;
	(void)memset(rp->features, 0xFF, sizeof(rp->features));
}

static void read_supported_commands(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				    uint16_t opcode)
{
	struct bt_hci_rp_read_supported_commands *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	(void)memset(rp->commands, 0xFF, sizeof(rp->commands));
	rp->status = 0x00;
}

/* Report the LE features the running test asked for. */
static void le_read_local_features(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				   uint16_t opcode)
{
	struct bt_hci_rp_le_read_local_features *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = 0x00;
	(void)memset(rp->features, 0, sizeof(rp->features));
	memcpy(rp->features, test_le_features, MIN(sizeof(rp->features), sizeof(test_le_features)));
}

static void le_read_supp_states(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				uint16_t opcode)
{
	struct bt_hci_rp_le_read_supp_states *rp;

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = 0x00;
	(void)memset(&rp->le_states, 0xFF, sizeof(rp->le_states));
}

/* Capture the LE event mask so that the test can check which events the host
 * allowed the Controller to generate.
 */
static void le_set_event_mask(struct net_buf *buf, struct net_buf **evt, uint8_t len,
			      uint16_t opcode)
{
	struct bt_hci_cp_le_set_event_mask *cp = (void *)buf->data;

	captured_le_event_mask = sys_get_le64(cp->events);

	generic_success(buf, evt, len, opcode);
}

/* Capture the scanning filter policy the host requests, so that the test can check
 * which of the four policies each option combination maps to.
 */
static void le_set_scan_param(struct net_buf *buf, struct net_buf **evt, uint8_t len,
			      uint16_t opcode)
{
	struct bt_hci_cp_le_set_scan_param *cp = (void *)buf->data;

	captured_filter_policy = cp->filter_policy;

	generic_success(buf, evt, len, opcode);
}

static void le_set_ext_scan_param(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				  uint16_t opcode)
{
	struct bt_hci_cp_le_set_ext_scan_param *cp = (void *)buf->data;

	captured_filter_policy = cp->filter_policy;

	generic_success(buf, evt, len, opcode);
}

static const struct cmd_handler cmds[] = {
	{BT_HCI_OP_READ_LOCAL_VERSION_INFO, sizeof(struct bt_hci_rp_read_local_version_info),
	 generic_success},
	{BT_HCI_OP_READ_SUPPORTED_COMMANDS, sizeof(struct bt_hci_rp_read_supported_commands),
	 read_supported_commands},
	{BT_HCI_OP_READ_LOCAL_FEATURES, sizeof(struct bt_hci_rp_read_local_features),
	 read_local_features},
	{BT_HCI_OP_READ_BD_ADDR, sizeof(struct bt_hci_rp_read_bd_addr), generic_success},
	{BT_HCI_OP_SET_EVENT_MASK, sizeof(struct bt_hci_evt_cc_status), generic_success},
	{BT_HCI_OP_LE_SET_EVENT_MASK, sizeof(struct bt_hci_evt_cc_status), le_set_event_mask},
	{BT_HCI_OP_LE_READ_LOCAL_FEATURES, sizeof(struct bt_hci_rp_le_read_local_features),
	 le_read_local_features},
	{BT_HCI_OP_LE_READ_SUPP_STATES, sizeof(struct bt_hci_rp_le_read_supp_states),
	 le_read_supp_states},
	{BT_HCI_OP_LE_RAND, sizeof(struct bt_hci_rp_le_rand), generic_success},
	{BT_HCI_OP_LE_SET_RANDOM_ADDRESS, sizeof(struct bt_hci_cp_le_set_random_address),
	 generic_success},
	{BT_HCI_OP_LE_SET_SCAN_PARAM, sizeof(struct bt_hci_evt_cc_status), le_set_scan_param},
	{BT_HCI_OP_LE_SET_SCAN_ENABLE, sizeof(struct bt_hci_evt_cc_status), generic_success},
	{BT_HCI_OP_LE_SET_EXT_SCAN_PARAM, sizeof(struct bt_hci_evt_cc_status),
	 le_set_ext_scan_param},
	{BT_HCI_OP_LE_SET_EXT_SCAN_ENABLE, sizeof(struct bt_hci_evt_cc_status), generic_success},
	{BT_HCI_OP_RESET, sizeof(struct bt_hci_evt_cc_status), generic_success},
};

static int cmd_handle(const struct bt_hci_transport *dev, struct net_buf *cmd)
{
	struct net_buf *evt = NULL;
	struct bt_hci_evt_cc_status *ccst;
	struct bt_hci_cmd_hdr *chdr;
	uint16_t opcode;
	bool handled = false;

	chdr = net_buf_pull_mem(cmd, sizeof(*chdr));
	opcode = sys_le16_to_cpu(chdr->opcode);

	ARRAY_FOR_EACH_PTR(cmds, entry) {
		if (entry->opcode == opcode) {
			entry->handler(cmd, &evt, entry->len, opcode);
			handled = true;
			break;
		}
	}

	if (!handled) {
		ccst = cmd_complete(&evt, sizeof(*ccst), opcode);
		ccst->status = BT_HCI_ERR_UNKNOWN_CMD;
	}

	if (evt != NULL) {
		bt_hci_recv(dev, evt);
	}

	return 0;
}

static int driver_open(const struct bt_hci_transport *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

/* Needed so that the test can cycle bt_disable()/bt_enable() to change the LE
 * features the fake Controller reports.
 */
static int driver_close(const struct bt_hci_transport *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int driver_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	uint8_t type = net_buf_pull_u8(buf);

	assert_int_equal(type, BT_HCI_H4_CMD);
	(void)cmd_handle(dev, buf);
	net_buf_unref(buf);

	return 0;
}

static const struct bt_hci_driver_api driver_api = {
	.open = driver_open,
	.close = driver_close,
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
;

/*
 * Pushing events to the host
 */

struct recv_job_data {
	struct bt_work work;
	os_sem_t *sync;
	struct net_buf *buf;
};

static struct recv_job_data job_data[CONFIG_BT_BUF_EVT_RX_COUNT];

#define job(buf) (&job_data[net_buf_id(buf)])

static void recv_job_cb(struct bt_work *item)
{
	const struct bt_hci_transport *dev = &test_driver;
	struct recv_job_data *data = CONTAINER_OF(item, struct recv_job_data, work);

	bt_hci_recv(dev, data->buf);
	osi_sem_give(data->sync);
}

/* Push an event to the host from the system workqueue and wait until it has
 * been consumed.
 */
static void recv_job_submit(struct net_buf *buf)
{
	os_sem_t sync_sem;

	job(buf)->buf = buf;
	bt_work_init(&job(buf)->work, recv_job_cb);
	osi_sem_init(&sync_sem, 0, 1);
	job(buf)->sync = &sync_sem;

	buf = net_buf_ref(buf);
	bt_work_submit(&job(buf)->work);
	osi_sem_take(&sync_sem, BT_FOREVER);
	assert_int_equal(bt_host_work_drain(), 0);
	/* The semaphore can wake before its posting work item has returned. */
	struct bt_work_sync injector_sync;

	bt_work_flush(&job(buf)->work, &injector_sync);
	assert_int_equal(osi_sem_deinit(&sync_sem), 0);
	net_buf_unref(buf);
}

static void direct_adv_info_create(struct bt_hci_evt_le_direct_adv_info *info,
				   const bt_addr_le_t *addr, const bt_addr_le_t *dir_addr,
				   int8_t rssi)
{
	info->evt_type = BT_HCI_ADV_DIRECT_IND;
	bt_addr_le_copy(&info->addr, addr);
	bt_addr_le_copy(&info->dir_addr, dir_addr);
	info->rssi = rssi;
}

/* Send an LE Directed Advertising Report event carrying @p num_reports report
 * entries, all with @p dir_addr as the target address. @p claimed_reports is
 * what the event says it carries, which lets a test build a truncated event.
 */
static void send_direct_adv_report(const bt_addr_le_t *dir_addr, uint8_t num_reports,
				   uint8_t claimed_reports)
{
	struct bt_hci_evt_le_meta_event *meta_evt;
	struct bt_hci_evt_le_direct_adv_info *info;
	struct net_buf *buf;
	uint8_t info_len = num_reports * sizeof(*info);

	buf = bt_buf_get_rx(BT_BUF_EVT, BT_FOREVER);

	evt_create(buf, BT_HCI_EVT_LE_META_EVENT, sizeof(*meta_evt) + 1 + info_len);
	meta_evt = net_buf_add(buf, sizeof(*meta_evt));
	meta_evt->subevent = BT_HCI_EVT_LE_DIRECT_ADV_REPORT;
	net_buf_add_u8(buf, claimed_reports);

	for (uint8_t i = 0; i < num_reports; i++) {
		info = net_buf_add(buf, sizeof(*info));
		direct_adv_info_create(info, &test_adv_addr, dir_addr, TEST_RSSI);
	}

	recv_job_submit(buf);
}

/* Send an ordinary LE Advertising Report event with no advertising data. */
static void send_adv_report(uint8_t evt_type)
{
	struct bt_hci_evt_le_meta_event *meta_evt;
	struct bt_hci_evt_le_advertising_info *info;
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_EVT, BT_FOREVER);

	evt_create(buf, BT_HCI_EVT_LE_META_EVENT, sizeof(*meta_evt) + 1 + sizeof(*info) + 1);
	meta_evt = net_buf_add(buf, sizeof(*meta_evt));
	meta_evt->subevent = BT_HCI_EVT_LE_ADVERTISING_REPORT;
	net_buf_add_u8(buf, 1);
	info = net_buf_add(buf, sizeof(*info));
	info->evt_type = evt_type;
	bt_addr_le_copy(&info->addr, &test_adv_addr);
	info->length = 0;
	net_buf_add_u8(buf, (uint8_t)TEST_RSSI);

	recv_job_submit(buf);
}

/* Send an LE Extended Advertising Report event for a legacy directed
 * advertisement with the given target address.
 */
static void send_ext_directed_report(const bt_addr_le_t *dir_addr)
{
	struct bt_hci_evt_le_meta_event *meta_evt;
	struct bt_hci_evt_le_ext_advertising_info *info;
	struct net_buf *buf;

	buf = bt_buf_get_rx(BT_BUF_EVT, BT_FOREVER);

	evt_create(buf, BT_HCI_EVT_LE_META_EVENT, sizeof(*meta_evt) + 1 + sizeof(*info));
	meta_evt = net_buf_add(buf, sizeof(*meta_evt));
	meta_evt->subevent = BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT;
	net_buf_add_u8(buf, 1);

	info = net_buf_add(buf, sizeof(*info));
	info->evt_type =
		sys_cpu_to_le16(BT_HCI_LE_ADV_EVT_TYPE_LEGACY | BT_HCI_LE_ADV_EVT_TYPE_CONN |
				BT_HCI_LE_ADV_EVT_TYPE_DIRECT);
	bt_addr_le_copy(&info->addr, &test_adv_addr);
	info->prim_phy = BT_HCI_LE_PHY_1M;
	info->sec_phy = 0;
	info->sid = BT_GAP_SID_INVALID;
	info->tx_power = BT_HCI_LE_ADV_TX_POWER_NO_PREF;
	info->rssi = TEST_RSSI;
	info->interval = 0;
	bt_addr_le_copy(&info->direct_addr, dir_addr);
	info->length = 0;

	recv_job_submit(buf);
}

static void start_scan(uint8_t options)
{
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = options,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};

	assert_int_equal(bt_le_scan_start(&param, NULL), 0);
}

/* Start the scanner with the given options, check which filter policy the host asked the
 * Controller for, then stop it again so the next combination starts cleanly.
 */
static void check_filter_policy(uint8_t options, uint8_t expected)
{
	captured_filter_policy = 0xFF;

	start_scan(options);

	assert_int_equal(captured_filter_policy, expected);

	assert_int_equal(bt_le_scan_stop(), 0);
}

static void *suite_setup(void)
{
	/* Extended Scanner Filter Policies supported. */
	(void)memset(test_le_features, 0, sizeof(test_le_features));
	test_le_features[0] = BIT(BT_LE_FEAT_BIT_EXT_SCAN);

	assert_int_equal(bt_enable(NULL), 0);
	assert_int_equal(bt_le_scan_cb_register(&scan_callbacks), 0);

	return NULL;
}

static void test_before(void *fixture)
{
	ARG_UNUSED(fixture);

	(void)memset(&last_report, 0, sizeof(last_report));
	recv_call_count = 0;
}

static void test_after(void *fixture)
{
	ARG_UNUSED(fixture);

	assert_int_equal(bt_le_scan_stop(), 0);
}



/* The host allows the Controller to generate the LE Directed Advertising Report
 * event when the Controller supports the Extended Scanner Filter Policies.
 */
static void bt_direct_adv_recv__test_event_mask_bit_set(void **cm_state)
{
	(void)cm_state;
	assert_true((captured_le_event_mask & BT_EVT_MASK_LE_DIRECT_ADV_REPORT) != 0);
}

/* Without Controller support for the Extended Scanner Filter Policies the host
 * neither unmasks the event nor lets the scanner be started with the option.
 */
static void bt_direct_adv_recv__test_no_controller_support(void **cm_state)
{
	(void)cm_state;
	struct bt_le_scan_param param = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = BT_LE_SCAN_OPT_EXT_FILTER_POLICY,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};

	assert_int_equal(bt_disable(), 0);

	test_le_features[0] &= ~BIT(BT_LE_FEAT_BIT_EXT_SCAN);
	captured_le_event_mask = 0;

	assert_int_equal(bt_enable(NULL), 0);

	assert_int_equal(captured_le_event_mask & BT_EVT_MASK_LE_DIRECT_ADV_REPORT, 0);
	assert_int_equal(bt_le_scan_start(&param, NULL), -ENOTSUP);

	/* Restore the Controller support for the remaining tests. */
	assert_int_equal(bt_disable(), 0);
	test_le_features[0] |= BIT(BT_LE_FEAT_BIT_EXT_SCAN);
	assert_int_equal(bt_enable(NULL), 0);
	assert_true((captured_le_event_mask & BT_EVT_MASK_LE_DIRECT_ADV_REPORT) != 0);
}

/* A directed advertisement whose target address the Controller could not
 * resolve is reported to the application with the target address marked as
 * unresolved.
 */
static void bt_direct_adv_recv__test_unresolved_target_is_reported(void **cm_state)
{
	(void)cm_state;
	start_scan(BT_LE_SCAN_OPT_EXT_FILTER_POLICY);

	send_direct_adv_report(&test_target_rpa, 1, 1);

	assert_int_equal(recv_call_count, 1);
	assert_int_equal(last_report.adv_type, BT_GAP_ADV_TYPE_ADV_DIRECT_IND);
	assert_int_equal(last_report.adv_props, BT_GAP_ADV_PROP_CONNECTABLE | BT_GAP_ADV_PROP_DIRECTED);
	assert_int_equal(last_report.rssi, TEST_RSSI);
	assert_int_equal(last_report.sid, BT_GAP_SID_INVALID);
	assert_int_equal(last_report.buf_len, 0);
	assert_true(bt_addr_le_eq(&last_report.addr, &test_adv_addr));

	assert_true(last_report.has_direct_addr);
	assert_int_equal(last_report.direct_addr.type, BT_ADDR_LE_UNRESOLVED);
	assert_memory_equal(last_report.direct_addr.a.val, test_target_rpa.a.val, BT_ADDR_SIZE);
}

/* A target address the Controller did resolve identifies the local device, so
 * it stays subject to the privacy rule that drops directed reports while
 * scanning with a non-resolvable private address.
 */
static void bt_direct_adv_recv__test_resolved_target_follows_privacy_rules(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t resolved_target = test_target_rpa;

	resolved_target.type = BT_ADDR_LE_RANDOM_ID;

	start_scan(BT_LE_SCAN_OPT_EXT_FILTER_POLICY);

	send_direct_adv_report(&resolved_target, 1, 1);

	if (!IS_ENABLED(CONFIG_BT_PRIVACY) && !IS_ENABLED(CONFIG_BT_SCAN_WITH_IDENTITY)) {
		assert_int_equal(recv_call_count, 0);
		return;
	}

	assert_int_equal(recv_call_count, 1);
	assert_true(last_report.has_direct_addr);
	assert_int_equal(last_report.direct_addr.type, BT_ADDR_LE_RANDOM);
	assert_memory_equal(last_report.direct_addr.a.val, test_target_rpa.a.val, BT_ADDR_SIZE);
}

/* Every report of an event carrying more than one is delivered. */
static void bt_direct_adv_recv__test_multiple_reports(void **cm_state)
{
	(void)cm_state;
	start_scan(BT_LE_SCAN_OPT_EXT_FILTER_POLICY);

	send_direct_adv_report(&test_target_rpa, 3, 3);

	assert_int_equal(recv_call_count, 3);
}

/* An event whose num_reports field claims more reports than the payload holds is
 * parsed up to the entries that are actually present. Those complete entries are
 * delivered, and parsing stops where the payload runs out.
 */
static void bt_direct_adv_recv__test_truncated_event_delivers_complete_reports(void **cm_state)
{
	(void)cm_state;
	start_scan(BT_LE_SCAN_OPT_EXT_FILTER_POLICY);

	send_direct_adv_report(&test_target_rpa, 1, 2);

	assert_int_equal(recv_call_count, 1);
}

/* An event with no report payload at all is discarded. */
static void bt_direct_adv_recv__test_empty_event_is_not_reported(void **cm_state)
{
	(void)cm_state;
	start_scan(BT_LE_SCAN_OPT_EXT_FILTER_POLICY);

	send_direct_adv_report(&test_target_rpa, 0, 1);

	assert_int_equal(recv_call_count, 0);
}

/* Reports are discarded while the application is not scanning. */
static void bt_direct_adv_recv__test_not_reported_when_not_scanning(void **cm_state)
{
	(void)cm_state;
	send_direct_adv_report(&test_target_rpa, 1, 1);

	assert_int_equal(recv_call_count, 0);
}

/* An ordinary LE Advertising Report carries no target address, so direct_addr
 * stays NULL for it.
 */
static void bt_direct_adv_recv__test_legacy_adv_report_has_no_target_address(void **cm_state)
{
	(void)cm_state;
	start_scan(BT_LE_SCAN_OPT_NONE);

	send_adv_report(BT_GAP_ADV_TYPE_ADV_IND);

	assert_int_equal(recv_call_count, 1);
	assert_false(last_report.has_direct_addr);
}

/* A directed advertisement whose target address the Controller resolved is reported
 * through the LE Advertising Report, which carries no target address, so it reaches the
 * application with BT_GAP_ADV_PROP_DIRECTED set and direct_addr NULL. Whether it is
 * delivered at all depends on the privacy rule in le_adv_recv().
 */
static void bt_direct_adv_recv__test_legacy_directed_report_has_no_target_address(void **cm_state)
{
	(void)cm_state;
	start_scan(BT_LE_SCAN_OPT_NONE);

	send_adv_report(BT_GAP_ADV_TYPE_ADV_DIRECT_IND);

	if (!IS_ENABLED(CONFIG_BT_PRIVACY) && !IS_ENABLED(CONFIG_BT_SCAN_WITH_IDENTITY)) {
		assert_int_equal(recv_call_count, 0);
		return;
	}

	assert_int_equal(recv_call_count, 1);
	assert_true((last_report.adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0U);
	assert_false(last_report.has_direct_addr);
}

/* The extended advertising report carries the target address directly. An
 * unresolved one must survive to the application unchanged: BT_ADDR_LE_UNRESOLVED
 * has the identity address bit set, so it must not be treated as an identity
 * address and converted.
 */
static void bt_direct_adv_recv__test_ext_report_unresolved_target(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t unresolved_target = test_target_rpa;

	if (!IS_ENABLED(CONFIG_BT_EXT_ADV)) {
		skip();
	}

	unresolved_target.type = BT_ADDR_LE_UNRESOLVED;

	start_scan(BT_LE_SCAN_OPT_EXT_FILTER_POLICY);

	send_ext_directed_report(&unresolved_target);

	assert_int_equal(recv_call_count, 1);
	assert_true(last_report.has_direct_addr);
	assert_int_equal(last_report.direct_addr.type, BT_ADDR_LE_UNRESOLVED);
	assert_memory_equal(last_report.direct_addr.a.val, test_target_rpa.a.val, BT_ADDR_SIZE);
}

/* All four scanning filter policies are reachable, and each option combination maps to
 * the right one. Which command carries the policy depends on CONFIG_BT_EXT_ADV, so the
 * configurations of this suite cover the legacy and the extended scan parameter commands
 * between them.
 *
 * Without CONFIG_BT_FILTER_ACCEPT_LIST there is no accept list to filter against, so the
 * host ignores BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST and asks for the unfiltered policy.
 */
static void bt_direct_adv_recv__test_scan_filter_policy(void **cm_state)
{
	(void)cm_state;
	const bool fal = IS_ENABLED(CONFIG_BT_FILTER_ACCEPT_LIST);

	check_filter_policy(BT_LE_SCAN_OPT_NONE, BT_HCI_LE_SCAN_FP_BASIC_NO_FILTER);
	check_filter_policy(BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST,
			    fal ? BT_HCI_LE_SCAN_FP_BASIC_FILTER
				: BT_HCI_LE_SCAN_FP_BASIC_NO_FILTER);
	check_filter_policy(BT_LE_SCAN_OPT_EXT_FILTER_POLICY, BT_HCI_LE_SCAN_FP_EXT_NO_FILTER);
	check_filter_policy(BT_LE_SCAN_OPT_EXT_FILTER_POLICY | BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST,
			    fal ? BT_HCI_LE_SCAN_FP_EXT_FILTER : BT_HCI_LE_SCAN_FP_EXT_NO_FILTER);
}

int bt_test_bt_direct_adv_recv_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	test_transport_init();
	*state = suite_setup();
	return 0;
}
static int bt_test_bt_direct_adv_recv_before(void **state)
{
	(void)state;
	test_before(*state);
	return 0;
}
int bt_test_bt_direct_adv_recv_after(void **state)
{
	(void)state;
	test_after(*state);
	return 0;
}

static const struct CMUnitTest test_cases_bt_direct_adv_recv[] = {
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_event_mask_bit_set, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_no_controller_support, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_unresolved_target_is_reported, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_resolved_target_follows_privacy_rules, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_multiple_reports, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_truncated_event_delivers_complete_reports, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_empty_event_is_not_reported, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_not_reported_when_not_scanning, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_legacy_adv_report_has_no_target_address, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_legacy_directed_report_has_no_target_address, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_ext_report_unresolved_target, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
	cmocka_unit_test_setup_teardown(bt_direct_adv_recv__test_scan_filter_policy, bt_test_bt_direct_adv_recv_before, bt_test_bt_direct_adv_recv_after),
};
BT_TEST_SUITE(bt_direct_adv_recv, test_cases_bt_direct_adv_recv, bt_test_bt_direct_adv_recv_setup, NULL);
