#include "support/test_support.h"
static const struct bt_hci_transport test_driver;
#include "host/hci_core.h"
/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#if defined(CONFIG_BT_PRIVACY)
#include <host/id.h>
#include <host/keys.h>
#endif


/* Large enough for the return parameters of initialization commands that are
 * irrelevant to these tests.
 */
#define GENERIC_RP_SIZE 64U

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
	struct bt_hci_evt_cc_status *rp;

	ARG_UNUSED(buf);

	rp = cmd_complete(evt, len, opcode);
	(void)memset(rp, 0, len);
	rp->status = BT_HCI_ERR_SUCCESS;
}

static bool rand_failure;

static void le_rand(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_le_rand *rp;
	static uint8_t rand_value;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	if (rand_failure) {
		rp->status = BT_HCI_ERR_HW_FAILURE;
		return;
	}

	rp->status = BT_HCI_ERR_SUCCESS;

	for (size_t i = 0U; i < ARRAY_SIZE(rp->rand); i++) {
		rp->rand[i] = rand_value++;
	}
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
	/* LE supported and BR/EDR not supported. */
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

static void command_status(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode)
{
	struct bt_hci_evt_cmd_status *cs;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	*evt = bt_buf_get_evt(BT_HCI_EVT_CMD_STATUS, false, BT_FOREVER);
	evt_create(*evt, BT_HCI_EVT_CMD_STATUS, sizeof(*cs));
	cs = net_buf_add(*evt, sizeof(*cs));
	cs->status = BT_HCI_ERR_SUCCESS;
	cs->ncmd = 1U;
	cs->opcode = sys_cpu_to_le16(opcode);
}

/* Whether legacy advertising and scanning are enabled */
static bool adv_enabled;
static bool scan_enabled;

/* The address the Host set last with LE Set Random Address */
static bt_addr_t random_addr;

static void le_set_adv_enable(struct net_buf *buf, struct net_buf **evt, uint8_t len,
			      uint16_t opcode)
{
	const struct bt_hci_cp_le_set_adv_enable *cp = net_buf_pull_mem(buf, sizeof(*cp));

	adv_enabled = cp->enable != 0U;
	generic_success(buf, evt, len, opcode);
}

static void le_set_scan_enable(struct net_buf *buf, struct net_buf **evt, uint8_t len,
			       uint16_t opcode)
{
	const struct bt_hci_cp_le_set_scan_enable *cp = net_buf_pull_mem(buf, sizeof(*cp));

	scan_enabled = cp->enable != 0U;
	generic_success(buf, evt, len, opcode);
}

static void le_set_random_address(struct net_buf *buf, struct net_buf **evt, uint8_t len,
				  uint16_t opcode)
{
	const struct bt_hci_cp_le_set_random_address *cp = net_buf_pull_mem(buf, sizeof(*cp));
	struct bt_hci_evt_cc_status *rp;

	/* Core v6.3, Vol 4, Part E, 7.8.4 */
	if (adv_enabled || scan_enabled) {
		rp = cmd_complete(evt, len, opcode);
		rp->status = BT_HCI_ERR_CMD_DISALLOWED;
		return;
	}

	bt_addr_copy(&random_addr, &cp->bdaddr);
	generic_success(buf, evt, len, opcode);
}

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
		BT_HCI_OP_LE_RAND,
		sizeof(struct bt_hci_rp_le_rand),
		le_rand,
	},
	{
		BT_HCI_OP_LE_CREATE_CONN,
		0U,
		command_status,
	},
	{
		BT_HCI_OP_LE_SET_RANDOM_ADDRESS,
		sizeof(struct bt_hci_evt_cc_status),
		le_set_random_address,
	},
	{
		BT_HCI_OP_LE_SET_ADV_ENABLE,
		sizeof(struct bt_hci_evt_cc_status),
		le_set_adv_enable,
	},
	{
		BT_HCI_OP_LE_SET_SCAN_ENABLE,
		sizeof(struct bt_hci_evt_cc_status),
		le_set_scan_enable,
	},
};

static void cmd_handle(const struct bt_hci_transport *dev, struct net_buf *cmd)
{
	struct net_buf *evt = NULL;
	struct bt_hci_cmd_hdr *chdr;
	uint16_t opcode;

	chdr = net_buf_pull_mem(cmd, sizeof(*chdr));
	opcode = sys_le16_to_cpu(chdr->opcode);

	for (size_t i = 0U; i < ARRAY_SIZE(cmds); i++) {
		if (cmds[i].opcode == opcode) {
			cmds[i].handler(cmd, &evt, cmds[i].len, opcode);
			bt_hci_recv(dev, evt);
			return;
		}
	}

	generic_success(cmd, &evt, GENERIC_RP_SIZE, opcode);
	bt_hci_recv(dev, evt);
}

/* What bt_hci_get_public_addr() returned the last time the transport was
 * opened, for the public-address channel test below.
 */
static bt_addr_t opened_with_addr;
static bool opened_with_public_addr;

static int driver_open(const struct bt_hci_transport *dev)
{
	if (IS_ENABLED(CONFIG_BT_HCI_SET_PUBLIC_ADDR)) {
		const bt_addr_t *addr = bt_hci_get_public_addr(dev);

		opened_with_public_addr = !bt_addr_eq(addr, BT_ADDR_ANY);
		bt_addr_copy(&opened_with_addr, addr);
	}

	return 0;
}

static int driver_close(const struct bt_hci_transport *dev)
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


static bt_addr_le_t test_addr(uint8_t value)
{
	bt_addr_le_t addr = {
		.type = BT_ADDR_LE_RANDOM,
		.a.val = {value, 0x22, 0x33, 0x44, 0x55, 0xC0},
	};

	return addr;
}

static size_t identity_count(void)
{
	size_t count = 0U;

	bt_id_get(NULL, &count);
	return count;
}

static void cleanup_secondary_identities(void)
{
	size_t count = identity_count();

	while (count > 1U) {
		const uint8_t id = (uint8_t)(count - 1U);
		int err;

		err = bt_id_delete(id);
		if (err == -EALREADY) {
			bt_addr_le_t addr = test_addr((uint8_t)(0x80U + id));

			/* Deleting the last identity shrinks the identity count, but an empty
			 * slot below it remains counted. Reclaim the slot before deleting it
			 * so cleanup can return to the default identity only.
			 */
			err = bt_id_reset(id, &addr, NULL);
			assert_int_equal(err, id);
			err = bt_id_delete(id);
		}

		assert_int_equal(err, 0);
		count = identity_count();
	}
}

static void *id_setup(void)
{
	if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
		bt_addr_le_t addr = test_addr(0x10U);
		uint8_t irk[BT_IRK_SIZE] = {1U};

		assert_int_equal(bt_id_create(&addr, irk), BT_ID_DEFAULT);
		assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, NULL), -EAGAIN);
	}

	int err;

	err = bt_enable(NULL);
	assert_int_equal(err, 0);
	assert_int_equal(identity_count(), 1U);

	return NULL;
}

static void id_before(void *fixture)
{
	ARG_UNUSED(fixture);

	cleanup_secondary_identities();
}

static void id_after(void *fixture)
{
	ARG_UNUSED(fixture);

	cleanup_secondary_identities();
}



static void bt_id_public_api__test_create_generated_addresses(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t generated_addr = *BT_ADDR_LE_ANY;
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);
	int id1;
	int id2;

	id1 = bt_id_create(NULL, NULL);
	assert_int_equal(id1, 1);

	id2 = bt_id_create(&generated_addr, NULL);
	assert_int_equal(id2, 2);
	assert_int_equal(generated_addr.type, BT_ADDR_LE_RANDOM);
	assert_true(BT_ADDR_IS_STATIC(&generated_addr.a));

	bt_id_get(addrs, &count);
	assert_int_equal(count, 3U);
	assert_int_equal(addrs[id1].type, BT_ADDR_LE_RANDOM);
	assert_true(BT_ADDR_IS_STATIC(&addrs[id1].a));
	assert_memory_equal(&addrs[id2], &generated_addr, sizeof(generated_addr));
	assert_false(bt_addr_le_eq(&addrs[BT_ID_DEFAULT], &addrs[id1]));
	assert_false(bt_addr_le_eq(&addrs[BT_ID_DEFAULT], &addrs[id2]));
	assert_false(bt_addr_le_eq(&addrs[id1], &addrs[id2]));
}

static void bt_id_public_api__test_create_errors_and_capacity(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t invalid_addr = test_addr(1U);
	bt_addr_le_t addr1 = test_addr(2U);
	bt_addr_le_t addr2 = test_addr(3U);
	bt_addr_le_t addr3 = test_addr(4U);
	bt_addr_le_t addr4 = test_addr(5U);
	bt_addr_le_t public_addr = test_addr(6U);
	uint8_t irk[BT_IRK_SIZE] = {1U};
	int err;
	int id;

	invalid_addr.a.val[5] = 0x40U;
	public_addr.type = BT_ADDR_LE_PUBLIC;
	assert_int_equal(bt_id_create(&invalid_addr, NULL), -EINVAL);
	err = bt_id_create(&public_addr, NULL);
	/* The same condition the Host applies: a public identity is taken when
	 * the controller can be given one.
	 */
	if (IS_ENABLED(CONFIG_BT_HCI_SET_PUBLIC_ADDR) || IS_ENABLED(CONFIG_BT_HCI_VS)) {
		assert_int_equal(err, -EALREADY);
	} else {
		assert_int_equal(err, -EINVAL);
	}

	if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
		assert_int_equal(bt_id_create(&addr1, irk), 1);
	} else {
		assert_int_equal(bt_id_create(&addr1, irk), -EINVAL);
	}

	if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
		id = 1;
	} else {
		id = bt_id_create(&addr1, NULL);
		assert_int_equal(id, 1);
	}
	assert_int_equal(bt_id_create(&addr1, NULL), -EALREADY);

	id = bt_id_create(&addr2, NULL);
	assert_int_equal(id, 2);
	id = bt_id_create(&addr3, NULL);
	assert_int_equal(id, 3);

	assert_int_equal(bt_id_create(&addr4, NULL), -ENOMEM);
	assert_int_equal(identity_count(), CONFIG_BT_ID_MAX);
}

static void bt_id_public_api__test_reset_errors_and_updates_identity(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t invalid_addr = test_addr(1U);
	bt_addr_le_t addr1 = test_addr(2U);
	bt_addr_le_t addr2 = test_addr(3U);
	bt_addr_le_t addr3 = test_addr(4U);
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	uint8_t irk[BT_IRK_SIZE] = {1U};
	size_t count = ARRAY_SIZE(addrs);
	uint8_t first_unused_id;
	int id1;
	int id2;

	id1 = bt_id_create(&addr1, NULL);
	id2 = bt_id_create(&addr2, NULL);
	assert_int_equal(id1, 1);
	assert_int_equal(id2, 2);
	first_unused_id = (uint8_t)identity_count();

	invalid_addr.a.val[5] = 0x40U;
	assert_int_equal(bt_id_reset((uint8_t)id1, &invalid_addr, NULL), -EINVAL);
	if (!IS_ENABLED(CONFIG_BT_PRIVACY)) {
		assert_int_equal(bt_id_reset((uint8_t)id1, &addr3, irk), -EINVAL);
	}
	assert_int_equal(bt_id_reset(BT_ID_DEFAULT, &addr3, NULL), -EINVAL);
	assert_int_equal(bt_id_reset(first_unused_id, &addr3, NULL), -EINVAL);
	assert_int_equal(bt_id_reset((uint8_t)id1, &addr2, NULL), -EALREADY);

	assert_int_equal(bt_id_reset((uint8_t)id1, &addr3, NULL), id1);
	bt_id_get(addrs, &count);
	assert_int_equal(count, 3U);
	assert_memory_equal(&addrs[id1], &addr3, sizeof(addr3));
}

static void bt_id_public_api__test_delete_errors_and_empty_slot(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t addr1 = test_addr(2U);
	bt_addr_le_t addr2 = test_addr(3U);
	bt_addr_le_t addr3 = test_addr(4U);
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = ARRAY_SIZE(addrs);
	uint8_t first_unused_id;
	int id1;
	int id2;

	id1 = bt_id_create(&addr1, NULL);
	id2 = bt_id_create(&addr2, NULL);
	assert_int_equal(id1, 1);
	assert_int_equal(id2, 2);
	first_unused_id = (uint8_t)identity_count();

	assert_int_equal(bt_id_delete(BT_ID_DEFAULT), -EINVAL);
	assert_int_equal(bt_id_delete(first_unused_id), -EINVAL);

	assert_int_equal(bt_id_delete((uint8_t)id1), 0);
	assert_int_equal(identity_count(), 3U);
	assert_int_equal(bt_id_delete((uint8_t)id1), -EALREADY);

	assert_int_equal(bt_id_reset((uint8_t)id1, &addr3, NULL), id1);
	bt_id_get(addrs, &count);
	assert_int_equal(count, 3U);
	assert_memory_equal(&addrs[id1], &addr3, sizeof(addr3));

	assert_int_equal(bt_id_delete((uint8_t)id2), 0);
	assert_int_equal(identity_count(), 2U);
}

static void bt_id_public_api__test_oob_rejects_invalid_identity(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t addr1 = test_addr(2U);
	bt_addr_le_t addr2 = test_addr(3U);
	struct bt_le_oob oob = {0};
	uint8_t first_unused_id = (uint8_t)identity_count();
	int id1;
	int id2;

	assert_int_equal(bt_le_oob_get_local(first_unused_id, &oob), -EINVAL);

	id1 = bt_id_create(&addr1, NULL);
	id2 = bt_id_create(&addr2, NULL);
	assert_int_equal(id1, 1);
	assert_int_equal(id2, 2);

	assert_int_equal(bt_id_delete((uint8_t)id1), 0);
	assert_int_equal(bt_le_oob_get_local((uint8_t)id1, &oob), -EINVAL);
}

#if defined(CONFIG_BT_PRIVACY)
static bool is_zero_irk(const uint8_t *irk)
{
	for (size_t i = 0U; i < BT_IRK_SIZE; i++) {
		if (irk[i] != 0U) {
			return false;
		}
	}

	return true;
}

static void bt_id_public_api__test_reset_irk_updates_key(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t before;
	bt_addr_le_t after;
	struct bt_le_oob old_oob = {0};
	struct bt_le_oob new_oob = {0};
	uint8_t old_irk[BT_IRK_SIZE] = {1U};
	const uint8_t expected_irk[BT_IRK_SIZE] = {2U};
	uint8_t supplied_irk[BT_IRK_SIZE] = {2U};
	uint8_t generated_irk[BT_IRK_SIZE] = {0};
	size_t count = 1U;

	bt_id_get(&before, &count);
	assert_int_equal(count, 1U);

	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, old_irk), 0);
	assert_int_equal(bt_le_oob_get_local(BT_ID_DEFAULT, &old_oob), 0);
	assert_true(bt_addr_le_is_rpa(&old_oob.addr));
	assert_true(bt_addr_matches_irk(&old_oob.addr.a, old_irk));

	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, supplied_irk), 0);
	assert_memory_equal(supplied_irk, expected_irk, sizeof(supplied_irk));

	count = 1U;
	bt_id_get(&after, &count);
	assert_int_equal(count, 1U);
	assert_memory_equal(&before, &after, sizeof(before));

	assert_int_equal(bt_le_oob_get_local(BT_ID_DEFAULT, &new_oob), 0);
	assert_true(bt_addr_le_is_rpa(&new_oob.addr));
	assert_true(bt_addr_matches_irk(&new_oob.addr.a, supplied_irk));
	assert_false(bt_addr_matches_irk(&new_oob.addr.a, old_irk));
	assert_false(bt_addr_le_eq(&old_oob.addr, &new_oob.addr));

	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, generated_irk), 0);
	assert_false(is_zero_irk(generated_irk));
}

#if defined(CONFIG_BT_RPA_SHARING)
static void bt_id_public_api__test_reset_irk_invalidates_cached_rpa(void **cm_state)
{
	(void)cm_state;
	struct bt_le_ext_adv *adv;
	struct bt_le_ext_adv_info info;
	bt_addr_le_t old_rpa;
	bt_addr_le_t new_rpa;
	uint8_t old_irk[BT_IRK_SIZE] = {1U};
	uint8_t new_irk[BT_IRK_SIZE] = {2U};
	int err;

	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, old_irk), 0);

	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, NULL, &adv);
	assert_int_equal(err, 0);
	err = bt_le_ext_adv_get_info(adv, &info);
	assert_int_equal(err, 0);
	bt_addr_le_copy(&old_rpa, info.addr);
	assert_true(bt_addr_matches_irk(&old_rpa.a, old_irk));
	assert_int_equal(bt_le_ext_adv_delete(adv), 0);

	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, new_irk), 0);
	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, NULL, &adv);
	assert_int_equal(err, 0);
	err = bt_le_ext_adv_get_info(adv, &info);
	assert_int_equal(err, 0);
	bt_addr_le_copy(&new_rpa, info.addr);
	assert_true(bt_addr_matches_irk(&new_rpa.a, new_irk));
	assert_false(bt_addr_matches_irk(&new_rpa.a, old_irk));
	assert_false(bt_addr_le_eq(&old_rpa, &new_rpa));
	assert_int_equal(bt_le_ext_adv_delete(adv), 0);
}
#endif /* defined(CONFIG_BT_RPA_SHARING) */

static void bt_id_public_api__test_reset_irk_rejects_random_failure(void **cm_state)
{
	(void)cm_state;
	uint8_t supplied_irk[BT_IRK_SIZE] = {1U};
	uint8_t failed_irk[BT_IRK_SIZE] = {0};
	int err;

	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, supplied_irk), 0);

	rand_failure = true;
	err = bt_id_reset_irk(BT_ID_DEFAULT, failed_irk);
	rand_failure = false;

	assert_int_equal(err, -EIO);
	assert_true(is_zero_irk(failed_irk));
}

#if defined(CONFIG_BT_SMP)
static void bt_id_public_api__test_reset_irk_rejects_bond(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t remote = test_addr(0x42U);
	struct bt_keys *keys;

	keys = bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &remote);
	assert_non_null(keys);
	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, NULL), -ENOTEMPTY);
	assert_int_equal(bt_unpair(BT_ID_DEFAULT, NULL), 0);
	assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, NULL), 0);
}
#endif

static void bt_id_public_api__test_reset_irk_rejects_empty_slot(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t addr1 = test_addr(0x44U);
	bt_addr_le_t addr2 = test_addr(0x45U);
	int id1;
	int id2;

	id1 = bt_id_create(&addr1, NULL);
	id2 = bt_id_create(&addr2, NULL);
	assert_int_equal(id1, 1);
	assert_int_equal(id2, 2);

	assert_int_equal(bt_id_delete((uint8_t)id1), 0);
	assert_int_equal(bt_id_reset_irk((uint8_t)id1, NULL), -EALREADY);
}

static void bt_id_public_api__test_reset_irk_rejects_active_roles(void **cm_state)
{
	(void)cm_state;
	int err;

	if (IS_ENABLED(CONFIG_BT_BROADCASTER)) {
		bt_addr_le_t secondary_addr = test_addr(0x46U);
		int secondary_id;

		secondary_id = bt_id_create(&secondary_addr, NULL);
		assert_int_equal(secondary_id, 1);

		err = bt_le_adv_start(BT_LE_ADV_NCONN, NULL, 0U, NULL, 0U);
		assert_int_equal(err, 0);
		assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, NULL), -EBUSY);
		assert_int_equal(bt_id_reset_irk((uint8_t)secondary_id, NULL), -EBUSY);
		assert_int_equal(bt_le_adv_stop(), 0);
	}

	if (IS_ENABLED(CONFIG_BT_OBSERVER)) {
		err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
		assert_int_equal(err, 0);
		assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, NULL), -EBUSY);
		assert_int_equal(bt_le_scan_stop(), 0);
	}

	if (IS_ENABLED(CONFIG_BT_CENTRAL)) {
		bt_addr_le_t remote = test_addr(0x43U);
		struct bt_conn *conn = NULL;

		err = bt_conn_le_create(&remote, BT_CONN_LE_CREATE_CONN,
					BT_LE_CONN_PARAM_DEFAULT, &conn);
		assert_int_equal(err, 0);
		assert_int_equal(bt_id_reset_irk(BT_ID_DEFAULT, NULL), -EBUSY);
		assert_int_equal(bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN), 0);
		bt_conn_unref(conn);
	}

	err = bt_id_reset_irk(BT_ID_DEFAULT, NULL);
	assert_int_equal(err, 0);
}

#if defined(CONFIG_BT_BROADCASTER) &&                                                              \
	(!defined(CONFIG_BT_EXT_ADV) || defined(CONFIG_BT_EXT_ADV_LEGACY_SUPPORT))
/* The test controller has no extended advertising, so this is the legacy advertiser */
#define NRPA_ADV_PARAM                                                                             \
	BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA, BT_GAP_ADV_FAST_INT_MIN_2,                         \
			BT_GAP_ADV_FAST_INT_MAX_2, NULL)

#if defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)
static void bt_id_public_api__test_nrpa_adv_keeps_nrpa_on_rotation(void **cm_state)
{
	(void)cm_state;
	struct bt_le_oob oob;
	bt_addr_t rotated;
	bt_addr_t nrpa;

	assert_int_equal(bt_le_set_rpa_timeout(1U), 0);
	/* Start a new RPA period, which uses the new timeout */
	assert_int_equal(bt_le_oob_get_local(BT_ID_DEFAULT, &oob), 0);

	assert_int_equal(bt_le_adv_start(NRPA_ADV_PARAM, NULL, 0U, NULL, 0U), 0);
	bt_addr_copy(&nrpa, &random_addr);
	assert_true(BT_ADDR_IS_NRPA(&nrpa));

	osi_sleep_ms(BT_MSEC(1500));
	bt_addr_copy(&rotated, &random_addr);

	assert_int_equal(bt_le_adv_stop(), 0);
	assert_int_equal(bt_le_set_rpa_timeout(CONFIG_BT_RPA_TIMEOUT), 0);

	assert_true(BT_ADDR_IS_NRPA(&rotated));
	assert_false(bt_addr_eq(&rotated, &nrpa));
}

#if defined(CONFIG_BT_OBSERVER)
static void bt_id_public_api__test_nrpa_adv_shares_nrpa_with_scanner(void **cm_state)
{
	(void)cm_state;
	struct bt_le_oob oob;
	bt_addr_t scan_addr;
	bt_addr_t rotated;
	bt_addr_t nrpa;
	int passive_err;
	int active_err;
	bool scanning;

	assert_int_equal(bt_le_set_rpa_timeout(1U), 0);
	/* Start a new RPA period, which uses the new timeout */
	assert_int_equal(bt_le_oob_get_local(BT_ID_DEFAULT, &oob), 0);

	assert_int_equal(bt_le_adv_start(NRPA_ADV_PARAM, NULL, 0U, NULL, 0U), 0);
	bt_addr_copy(&nrpa, &random_addr);

	passive_err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
	if (passive_err == 0) {
		(void)bt_le_scan_stop();
	}
	active_err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, NULL);
	bt_addr_copy(&scan_addr, &random_addr);

	osi_sleep_ms(BT_MSEC(1500));
	bt_addr_copy(&rotated, &random_addr);
	scanning = scan_enabled;

	if (active_err == 0) {
		(void)bt_le_scan_stop();
	}
	assert_int_equal(bt_le_adv_stop(), 0);
	assert_int_equal(bt_le_set_rpa_timeout(CONFIG_BT_RPA_TIMEOUT), 0);

	assert_int_equal(passive_err, 0);
	assert_int_equal(active_err, 0);
	assert_true(bt_addr_eq(&scan_addr, &nrpa));
	assert_true(BT_ADDR_IS_NRPA(&rotated));
	assert_false(bt_addr_eq(&rotated, &nrpa));
	assert_true(scanning);
}
#endif /* defined(CONFIG_BT_OBSERVER) */
#endif /* defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC) */

#if defined(CONFIG_BT_CENTRAL)
static void bt_id_public_api__test_nrpa_adv_rejects_conn_create(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t remote = test_addr(0x47U);
	struct bt_conn *conn = NULL;
	int err;

	assert_int_equal(bt_le_adv_start(NRPA_ADV_PARAM, NULL, 0U, NULL, 0U), 0);

	err = bt_conn_le_create(&remote, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
	if (conn != NULL) {
		(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		bt_conn_unref(conn);
	}

	assert_int_equal(bt_le_adv_stop(), 0);
	assert_int_equal(err, -EINVAL);
}
#endif /* defined(CONFIG_BT_CENTRAL) */

static void bt_id_public_api__test_nrpa_adv_rejects_oob_get_local(void **cm_state)
{
	(void)cm_state;
	struct bt_le_oob oob;
	int err;

	assert_int_equal(bt_le_adv_start(NRPA_ADV_PARAM, NULL, 0U, NULL, 0U), 0);
	err = bt_le_oob_get_local(BT_ID_DEFAULT, &oob);
	assert_int_equal(bt_le_adv_stop(), 0);

	assert_int_equal(err, -EINVAL);
}

#if defined(CONFIG_BT_CENTRAL) && defined(CONFIG_BT_SMP)
static void bt_id_public_api__test_nrpa_adv_rejects_conn_create_with_host_resolving(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t remote = test_addr(0x48U);
	struct bt_conn *conn = NULL;
	struct bt_keys *keys;
	int unpair_err;
	int stop_err;
	int err;

	/* The test controller has no resolving list, so the bond makes
	 * bt_conn_le_create() resolve the peer's address in the Host.
	 */
	keys = bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &remote);
	assert_non_null(keys);
	bt_id_add(keys);

	assert_int_equal(bt_le_adv_start(NRPA_ADV_PARAM, NULL, 0U, NULL, 0U), 0);

	err = bt_conn_le_create(&remote, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &conn);
	if (conn != NULL) {
		(void)bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		bt_conn_unref(conn);
	}

	stop_err = bt_le_adv_stop();
	unpair_err = bt_unpair(BT_ID_DEFAULT, NULL);

	assert_int_equal(err, -EINVAL);
	assert_int_equal(stop_err, 0);
	assert_int_equal(unpair_err, 0);
}
#endif /* defined(CONFIG_BT_CENTRAL) && defined(CONFIG_BT_SMP) */
#endif /* CONFIG_BT_BROADCASTER && (!CONFIG_BT_EXT_ADV || CONFIG_BT_EXT_ADV_LEGACY_SUPPORT) */

#if defined(CONFIG_BT_OBSERVER) && !defined(CONFIG_BT_BROADCASTER) &&                              \
	defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)
/* Scan through one expiry of the RPA timeout. Report whether
 * rpa_timeout() started the RPA timer again.
 */
static int scan_through_rpa_timeout(const struct bt_le_scan_param *param, uint16_t rpa_timeout_s,
				    bool *pending)
{
	int err;

	err = bt_le_scan_start(param, NULL);
	if (err != 0) {
		return err;
	}

	/* Wait half a second past the RPA timeout: rpa_timeout() has run,
	 * and the next expiry is still ahead.
	 */
	(void)osi_sleep_ms(BT_MSEC(rpa_timeout_s * MSEC_PER_SEC + 500U));
	*pending = bt_work_delayable_is_pending(&bt_dev.rpa_update);

	return bt_le_scan_stop();
}

static void bt_id_public_api__test_rpa_timer_stops_without_active_scanner(void **cm_state)
{
	(void)cm_state;
	const uint16_t rpa_timeout_s = 1U;
	struct bt_le_oob oob;
	bool passive_pending = false;
	bool active_pending = false;
	int passive_err;
	int active_err;

	assert_int_equal(bt_le_set_rpa_timeout(rpa_timeout_s), 0);
	/* Start a new RPA period, which uses the new timeout */
	assert_int_equal(bt_le_oob_get_local(BT_ID_DEFAULT, &oob), 0);

	passive_err = scan_through_rpa_timeout(BT_LE_SCAN_PASSIVE, rpa_timeout_s, &passive_pending);
	active_err = scan_through_rpa_timeout(BT_LE_SCAN_ACTIVE, rpa_timeout_s, &active_pending);

	assert_int_equal(bt_le_set_rpa_timeout(CONFIG_BT_RPA_TIMEOUT), 0);

	assert_int_equal(passive_err, 0);
	assert_int_equal(active_err, 0);
	assert_false(passive_pending);
	assert_true(active_pending);
}
#endif /* CONFIG_BT_OBSERVER && !CONFIG_BT_BROADCASTER && CONFIG_BT_RPA_TIMEOUT_DYNAMIC */
#endif /* defined(CONFIG_BT_PRIVACY) */

/* The public identity address reaches the driver through the common driver
 * data, so that a driver can apply it while opening the transport rather than
 * from the setup() op. The Host sets it before every open, which includes
 * clearing it when the application has not created a public identity.
 */


static void bt_hci_public_addr__test_public_addr_reaches_the_driver(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t public_addr = {
		.type = BT_ADDR_LE_PUBLIC,
		.a.val = {0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U},
	};
	int err;

	if (!IS_ENABLED(CONFIG_BT_HCI_SET_PUBLIC_ADDR)) {
		skip();
	}

	/* A public identity can only be the first one, so start from a closed
	 * transport whether or not another suite has opened it.
	 */
	err = bt_disable();
	assert_true(err == 0 || err == -EALREADY);

	assert_int_equal(bt_id_create(&public_addr, NULL), BT_ID_DEFAULT);

	assert_int_equal(bt_enable(NULL), 0);
	assert_true(opened_with_public_addr);
	assert_memory_equal(&opened_with_addr, &public_addr.a, sizeof(opened_with_addr));

	/* bt_disable() drops the identities, so the next open must not be given
	 * the address this one left behind.
	 */
	assert_int_equal(bt_disable(), 0);
	assert_int_equal(bt_enable(NULL), 0);
	assert_false(opened_with_public_addr);
	/* The documented value for "no address", which is not BT_ADDR_NONE */
	assert_true(bt_addr_eq(&opened_with_addr, BT_ADDR_ANY));

	assert_int_equal(bt_disable(), 0);
}

int bt_test_bt_hci_public_addr_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	test_transport_init();
	*state = NULL;
	return 0;
}



static const struct CMUnitTest test_cases_bt_hci_public_addr[] = {
	cmocka_unit_test(bt_hci_public_addr__test_public_addr_reaches_the_driver),
};
BT_TEST_SUITE(bt_hci_public_addr, test_cases_bt_hci_public_addr, bt_test_bt_hci_public_addr_setup, NULL);

int bt_test_bt_id_public_api_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	test_transport_init();
	*state = id_setup();
	return 0;
}
static int bt_test_bt_id_public_api_before(void **state)
{
	(void)state;
	id_before(*state);
	return 0;
}
int bt_test_bt_id_public_api_after(void **state)
{
	(void)state;
	id_after(*state);
	return 0;
}

static const struct CMUnitTest test_cases_bt_id_public_api[] = {
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_create_generated_addresses, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_create_errors_and_capacity, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_errors_and_updates_identity, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_delete_errors_and_empty_slot, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_oob_rejects_invalid_identity, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#if (defined(CONFIG_BT_PRIVACY))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_irk_updates_key, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_RPA_SHARING))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_irk_invalidates_cached_rpa, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_irk_rejects_random_failure, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_SMP))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_irk_rejects_bond, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_irk_rejects_empty_slot, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_reset_irk_rejects_active_roles, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)) && (defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_nrpa_adv_keeps_nrpa_on_rotation, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)) && (defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)) && (defined(CONFIG_BT_OBSERVER))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_nrpa_adv_shares_nrpa_with_scanner, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)) && (defined(CONFIG_BT_CENTRAL))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_nrpa_adv_rejects_conn_create, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_nrpa_adv_rejects_oob_get_local, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC)) && (defined(CONFIG_BT_CENTRAL) && defined(CONFIG_BT_SMP))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_nrpa_adv_rejects_conn_create_with_host_resolving, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
#if (defined(CONFIG_BT_PRIVACY)) && (defined(CONFIG_BT_OBSERVER) && !defined(CONFIG_BT_BROADCASTER) &&                              defined(CONFIG_BT_RPA_TIMEOUT_DYNAMIC))
	cmocka_unit_test_setup_teardown(bt_id_public_api__test_rpa_timer_stops_without_active_scanner, bt_test_bt_id_public_api_before, bt_test_bt_id_public_api_after),
#endif
};
BT_TEST_SUITE(bt_id_public_api, test_cases_bt_id_public_api, bt_test_bt_id_public_api_setup, NULL);
