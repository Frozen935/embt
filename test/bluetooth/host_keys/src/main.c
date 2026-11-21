#include "support/test_support.h"
static const struct bt_hci_transport test_driver;
#include "host/hci_core.h"
/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/drivers/bluetooth.h>

#include <embt/net_buf.h>
#include <embt/settings/settings.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include <host/keys.h>
#include <host/settings.h>
#include <embt/base/byteorder.h>


#define GENERIC_RP_SIZE 64U

#define NUM_PEERS 2U

static const bt_addr_le_t peers[NUM_PEERS] = {
	{
		.type = BT_ADDR_LE_PUBLIC,
		.a.val = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
	},
	{
		.type = BT_ADDR_LE_PUBLIC,
		.a.val = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16},
	},
};

/* Third peer: the pool holds CONFIG_BT_MAX_PAIRED (2) entries, so a third
 * address is needed for the full-pool test cases.
 */
static const bt_addr_le_t peer_extra = {
	.type = BT_ADDR_LE_PUBLIC,
	.a.val = {0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf},
};

static uint8_t rl_adds[NUM_PEERS];
static uint8_t rl_rejects[NUM_PEERS];

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

static void le_rand(struct net_buf *buf, struct net_buf **evt, uint8_t len, uint16_t opcode)
{
	struct bt_hci_rp_le_rand *rp;
	static uint8_t rand_value;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
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
	rp->features[0] = BIT(BT_LE_FEAT_BIT_PRIVACY);
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

static void le_read_rl_size(struct net_buf *buf, struct net_buf **evt, uint8_t len,
			    uint16_t opcode)
{
	struct bt_hci_rp_le_read_rl_size *rp;

	ARG_UNUSED(buf);
	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;
	rp->rl_size = CONFIG_TEST_RL_SIZE;
}

static void le_add_dev_to_rl(struct net_buf *buf, struct net_buf **evt, uint8_t len,
			     uint16_t opcode)
{
	struct bt_hci_cp_le_add_dev_to_rl *cp = (void *)buf->data;
	struct bt_hci_evt_cc_status *rp;

	ARG_UNUSED(len);

	rp = cmd_complete(evt, sizeof(*rp), opcode);
	rp->status = BT_HCI_ERR_SUCCESS;

	for (size_t i = 0U; i < NUM_PEERS; i++) {
		if (!bt_addr_le_eq(&cp->peer_id_addr, &peers[i])) {
			continue;
		}

		rl_adds[i]++;

		/* The Controller rejects an entry that is already in the list. */
		if (rl_adds[i] > 1U) {
			rl_rejects[i]++;
			rp->status = BT_HCI_ERR_INVALID_PARAM;
		}
	}
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
		BT_HCI_OP_LE_READ_RL_SIZE,
		sizeof(struct bt_hci_rp_le_read_rl_size),
		le_read_rl_size,
	},
	{
		BT_HCI_OP_LE_ADD_DEV_TO_RL,
		sizeof(struct bt_hci_evt_cc_status),
		le_add_dev_to_rl,
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

static int driver_open(const struct bt_hci_transport *dev)
{
	ARG_UNUSED(dev);

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


static void store_bond(size_t idx)
{
	struct bt_keys *keys;

	keys = bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[idx]);
	assert_non_null(keys);
	(void)memset(keys->irk.val, 0x5A + idx, sizeof(keys->irk.val));
	assert_int_equal(bt_keys_store(keys), 0);
}

static void *keys_setup(void)
{
	int err;

	err = bt_enable(NULL);
	assert_true(err == 0 || err == -EALREADY);

	store_bond(0U);
	/* Drop the in-RAM copy so that the first settings_load() has to restore it. */
	bt_keys_reset();

	return NULL;
}

static void load_and_advertise(void)
{
	assert_int_equal(settings_load(), 0);
	assert_int_equal(bt_le_adv_start(BT_LE_ADV_NCONN, NULL, 0, NULL, 0), 0);
	assert_int_equal(bt_le_adv_stop(), 0);
}



static void bt_keys_commit__test_settings_reload_does_not_add_irk_twice(void **cm_state)
{
	(void)cm_state;
	const uint8_t expected_adds = (CONFIG_TEST_RL_SIZE > 0) ? 1U : 0U;
	struct bt_keys *keys;

	assert_null(bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]));

	load_and_advertise();

	keys = bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]);
	assert_non_null(keys);
	assert_true((keys->state & BT_KEYS_ID_ADDED) != 0U);
	assert_int_equal(bt_dev.le.rl_entries, 1U);
	assert_int_equal(rl_adds[0], expected_adds);

	store_bond(1U);

	load_and_advertise();

	assert_int_equal(rl_adds[0], expected_adds);
	assert_int_equal(rl_adds[1], expected_adds);
	assert_int_equal(rl_rejects[0], 0U);
	assert_int_equal(rl_rejects[1], 0U);
	assert_int_equal(bt_dev.le.rl_entries, 2U);
}

/* ZTest does not guarantee test execution order within a suite, so reset
 * the key pool before every test to keep them independent.
 */
static void reset_key_pool(void *fixture)
{
	ARG_UNUSED(fixture);

	bt_keys_reset();
}



/* bt_keys_get_addr(): allocate a slot and find it again. */
static void bt_keys_pool__test_get_addr_allocates_slot(void **cm_state)
{
	(void)cm_state;
	const struct bt_keys *keys = bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]);

	assert_non_null(keys);
	assert_ptr_equal(keys, bt_keys_find_addr(BT_ID_DEFAULT, &peers[0]));
	assert_int_equal(keys->keys, 0U);
}

/* bt_keys_get_addr(): a full pool rejects new addresses, but an existing
 * entry must still be returned — the find-first path must not be confused
 * with allocation.
 */
static void bt_keys_pool__test_get_addr_returns_existing_when_full(void **cm_state)
{
	(void)cm_state;
	const struct bt_keys *keys;

	if (IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)) {
		/* Under CONFIG_BT_KEYS_OVERWRITE_OLDEST a full pool evicts the
		 * least recently used entry instead of rejecting; that behavior
		 * is covered by the bt_keys_oldest suite.
		 */
		skip();
	}

	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]));
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[1]));

	keys = bt_keys_get_addr(BT_ID_DEFAULT, &peer_extra);
	assert_null(keys);

	assert_ptr_equal(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]), bt_keys_find_addr(BT_ID_DEFAULT, &peers[0]));
}

/* bt_keys_get_type(): same ref on repeated calls, key type bit set, and no
 * slot handed out from a full pool.
 */
static void bt_keys_pool__test_get_type_existing_and_full(void **cm_state)
{
	(void)cm_state;
	const struct bt_keys *keys;

	if (IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)) {
		/* Under CONFIG_BT_KEYS_OVERWRITE_OLDEST a full pool evicts the
		 * least recently used entry instead of rejecting; that behavior
		 * is covered by the bt_keys_oldest suite.
		 */
		skip();
	}

	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]));
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[1]));

	keys = bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]);
	assert_non_null(keys);
	assert_ptr_equal(keys, bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]));
	assert_true((keys->keys & BT_KEYS_IRK) != 0U);

	assert_null(bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &peer_extra));
}

static void bond_count_cb(const struct bt_bond_info *info, void *user_data)
{
	size_t *count = user_data;

	ARG_UNUSED(info);
	(*count)++;
}

/* bt_keys_find()/find_addr(): miss on empty pool, miss for a type that is
 * not set, hit, and the public bond enumeration agrees.
 */
static void bt_keys_pool__test_find_and_foreach(void **cm_state)
{
	(void)cm_state;
	const struct bt_keys *keys;
	size_t count = 0;

	assert_null(bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]));

	keys = bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]);
	assert_non_null(keys);

	assert_ptr_equal(bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &peers[0]), keys);
	assert_null(bt_keys_find(BT_KEYS_LTK, BT_ID_DEFAULT, &peers[0]));
	assert_ptr_equal(bt_keys_find_addr(BT_ID_DEFAULT, &peers[0]), keys);

	bt_foreach_bond(BT_ID_DEFAULT, bond_count_cb, &count);
	assert_int_equal(count, 1U);
}

/* ------------------------------------------------------------------------
 * Settings migration / edge-case coverage (keys_set branches)
 *
 * Each test writes a crafted entry directly to the settings backend and
 * reloads only that entry with settings_load_subtree(), so every keys_set
 * branch is reached through the real persistence path without interference
 * from other stored bonds.
 * ---------------------------------------------------------------------
 */

static const bt_addr_le_t migration_peers[] = {
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25}},
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35}},
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45}},
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x50, 0x51, 0x52, 0x53, 0x54, 0x55}},
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x60, 0x61, 0x62, 0x63, 0x64, 0x65}},
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}},
	{.type = BT_ADDR_LE_PUBLIC, .a.val = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85}},
};

/* Must match the host's STORAGE_CFG_VERSION. If the host bumps it, this
 * test fails loudly - update this constant.
 */
#define STORAGE_CFG_VERSION_CURRENT 17U

static void migration_blob_init(struct bt_keys *keys, const bt_addr_le_t *addr, uint16_t types,
				uint8_t flags)
{
	uint32_t cfg_flags = 0;

	/* cfg_flags must match the host's STORAGE_CFG_FLAGS, which is derived
	 * from the same Kconfig options.
	 */
	if (IS_ENABLED(CONFIG_BT_SIGNING)) {
		cfg_flags |= BT_KEYS_CFG_SIGNING;
	}
	if (IS_ENABLED(CONFIG_BT_SMP_SC_PAIR_ONLY)) {
		cfg_flags |= BT_KEYS_CFG_SC_PAIR_ONLY;
	}
	if (IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)) {
		cfg_flags |= BT_KEYS_CFG_OVERWRITE_OLDEST;
	}

	(void)memset(keys, 0, sizeof(*keys));
	keys->id = BT_ID_DEFAULT;
	bt_addr_le_copy(&keys->addr, addr);
	keys->cfg_version = STORAGE_CFG_VERSION_CURRENT;
	sys_put_le24(cfg_flags, keys->cfg_flags);
	keys->enc_size = 16;
	keys->keys = types;
	keys->flags = flags;
}

/* The suite must also pass when run without the other suites, so it
 * initializes the settings subsystem itself.
 */
static void *settings_suite_setup(void)
{
	int err;

	err = bt_settings_init();
	assert_int_equal(err, 0);

	return NULL;
}

static void settings_suite_teardown(void *fixture)
{
	char name[BT_SETTINGS_KEY_MAX];
	int err;

	ARG_UNUSED(fixture);

	bt_keys_reset();
	for (size_t i = 0; i < ARRAY_SIZE(migration_peers); i++) {
		assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[i],
						  NULL), 0);
		err = settings_save_one(name, NULL, 0);
		assert_true(err == 0 || err == -ENOENT);
	}
}



/* Well-formed stored bond restores through the real load path. */
static void bt_keys_settings__test_load_stored_bond(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	const struct bt_keys *keys;
	char name[BT_SETTINGS_KEY_MAX];

	migration_blob_init(&stored, &migration_peers[0], BT_KEYS_IRK, 0);
	for (int i = 0; i < 16; i++) {
		stored.irk.val[i] = 0xa0 + i;
	}

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[0], NULL), 0);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	keys = bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &migration_peers[0]);
	assert_non_null(keys);
	assert_memory_equal(keys->irk.val, stored.irk.val, 16);
}

/* A zero-length entry is settings' delete representation: it removes an
 * existing bond (covers the keys_set delete branch).
 */
static void bt_keys_settings__test_load_delete_marker_removes_bond(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	const struct bt_keys *keys;
	char name[BT_SETTINGS_KEY_MAX];

	migration_blob_init(&stored, &migration_peers[1], BT_KEYS_IRK, 0);

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[1], NULL), 0);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	keys = bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &migration_peers[1]);
	assert_non_null(keys);

	/* Drop the NVS copy and deliver a zero-length entry through the
	 * runtime backend to reach the keys_set delete branch.
	 */
	assert_int_equal(settings_save_one(name, NULL, 0), 0);
	assert_int_equal(settings_runtime_set(name, stored.storage_start, 0), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &migration_peers[1]));
}

/* Old storage format: no cfg_version/cfg_flags, blob starts at enc_size. */
static void bt_keys_settings__test_load_old_format_bond(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	const struct bt_keys *keys;
	char name[BT_SETTINGS_KEY_MAX];
	const size_t old_len = sizeof(struct bt_keys) - offsetof(struct bt_keys, enc_size);

	migration_blob_init(&stored, &migration_peers[2], BT_KEYS_IRK, 0);
	for (int i = 0; i < 16; i++) {
		stored.irk.val[i] = 0xb0 + i;
	}

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[2], NULL), 0);
	assert_int_equal(settings_save_one(name, &stored.enc_size, old_len), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	keys = bt_keys_find(BT_KEYS_IRK, BT_ID_DEFAULT, &migration_peers[2]);
	assert_non_null(keys);
	assert_memory_equal(keys->irk.val, stored.irk.val, 16);
}

/* Neither the current nor the compatible length: rejected. */
static void bt_keys_settings__test_load_rejects_invalid_length(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	char name[BT_SETTINGS_KEY_MAX];

	migration_blob_init(&stored, &migration_peers[3], BT_KEYS_IRK, 0);

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[3], NULL), 0);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN - 1), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &migration_peers[3]));
}

/* cfg_flags that do not match the current build: rejected. The toggle
 * mismatches regardless of which flags the build enables.
 */
static void bt_keys_settings__test_load_rejects_cfg_flags_mismatch(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	char name[BT_SETTINGS_KEY_MAX];

	migration_blob_init(&stored, &migration_peers[4], BT_KEYS_IRK, 0);
	stored.cfg_flags[0] ^= BT_KEYS_CFG_SIGNING;

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[4], NULL), 0);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &migration_peers[4]));
}

/* A cfg_version other than the current one: rejected. */
static void bt_keys_settings__test_load_rejects_cfg_version_mismatch(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	char name[BT_SETTINGS_KEY_MAX];

	migration_blob_init(&stored, &migration_peers[6], BT_KEYS_IRK, 0);
	stored.cfg_version = STORAGE_CFG_VERSION_CURRENT + 1;

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[6], NULL), 0);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &migration_peers[6]));
}

/* Legacy authenticated flag without SC or OOB pairing: downgraded to
 * unauthenticated on load. With SC set, it is retained.
 */
static void bt_keys_settings__test_load_downgrades_unprovable_authentication(void **cm_state)
{
	(void)cm_state;
	struct bt_keys stored;
	const struct bt_keys *keys;
	char name[BT_SETTINGS_KEY_MAX];

	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &migration_peers[5], NULL), 0);

	migration_blob_init(&stored, &migration_peers[5], BT_KEYS_LTK, BT_KEYS_AUTHENTICATED);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	keys = bt_keys_find(BT_KEYS_LTK, BT_ID_DEFAULT, &migration_peers[5]);
	assert_non_null(keys);
	assert_int_equal(keys->flags & BT_KEYS_AUTHENTICATED, 0U);

	migration_blob_init(&stored, &migration_peers[5], BT_KEYS_LTK,
			    BT_KEYS_AUTHENTICATED | BT_KEYS_SC);
	assert_int_equal(settings_save_one(name, stored.storage_start, BT_KEYS_STORAGE_LEN), 0);
	assert_int_equal(settings_load_subtree(name), 0);

	keys = bt_keys_find(BT_KEYS_LTK, BT_ID_DEFAULT, &migration_peers[5]);
	assert_non_null(keys);
	assert_int_not_equal(keys->flags & BT_KEYS_AUTHENTICATED, 0U);
}

#if defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
/* Deliver an HCI event to the host. Takes ownership of buf. */
static void drv_recv(struct net_buf *buf)
{
	bt_hci_recv(&test_driver, buf);
}

/* ------------------------------------------------------------------------
 * CONFIG_BT_KEYS_OVERWRITE_OLDEST pool eviction coverage.
 *
 * In-use entries are simulated with fake incoming connections: the host
 * advertises, then an LE Connection Complete event with the peripheral
 * role is injected through the test HCI driver.
 * ---------------------------------------------------------------------
 */

static os_sem_t connected_sem;
static os_sem_t disconnected_sem;

static struct bt_conn *connected_peers[2];
static size_t connected_count;

static void connected_cb(struct bt_conn *conn, uint8_t err)
{
	assert_int_equal(err, 0U);
	if (connected_count < ARRAY_SIZE(connected_peers)) {
		connected_peers[connected_count++] = bt_conn_ref(conn);
	}
	osi_sem_give(&connected_sem);
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(reason);
	osi_sem_give(&disconnected_sem);
}

BT_CONN_CB_DEFINE(test_conn_cb) = {
	.connected = connected_cb,
	.disconnected = disconnected_cb,
};

static void inject_conn_complete(const bt_addr_le_t *peer, uint16_t handle)
{
	struct net_buf *buf = bt_buf_get_evt(BT_HCI_EVT_LE_META_EVENT, false, BT_FOREVER);
	struct bt_hci_evt_le_meta_event *mev;
	struct bt_hci_evt_le_conn_complete *cc;

	evt_create(buf, BT_HCI_EVT_LE_META_EVENT,
		   sizeof(struct bt_hci_evt_le_meta_event) +
			   sizeof(struct bt_hci_evt_le_conn_complete));
	mev = net_buf_add(buf, sizeof(*mev));
	cc = net_buf_add(buf, sizeof(*cc));

	mev->subevent = BT_HCI_EVT_LE_CONN_COMPLETE;
	cc->status = BT_HCI_ERR_SUCCESS;
	cc->handle = sys_cpu_to_le16(handle);
	cc->role = BT_HCI_ROLE_PERIPHERAL;
	bt_addr_le_copy(&cc->peer_addr, peer);
	cc->interval = sys_cpu_to_le16(24U);
	cc->latency = 0U;
	cc->supv_timeout = sys_cpu_to_le16(400U);
	cc->clock_accuracy = 0U;

	drv_recv(buf);
}

static void inject_disconn_complete(uint16_t handle)
{
	struct net_buf *buf = bt_buf_get_evt(BT_HCI_EVT_DISCONN_COMPLETE, false, BT_FOREVER);
	struct bt_hci_evt_disconn_complete *dc;

	evt_create(buf, BT_HCI_EVT_DISCONN_COMPLETE, sizeof(struct bt_hci_evt_disconn_complete));
	dc = net_buf_add(buf, sizeof(*dc));

	dc->status = BT_HCI_ERR_SUCCESS;
	dc->handle = sys_cpu_to_le16(handle);
	dc->reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;

	drv_recv(buf);
}

static void connect_peer(const bt_addr_le_t *peer, uint16_t handle)
{
	assert_int_equal(bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, NULL, 0, NULL, 0), 0);
	inject_conn_complete(peer, handle);
	assert_int_equal(osi_sem_take(&connected_sem, BT_SECONDS(1)), 0);
	assert_int_equal(bt_le_adv_stop(), 0);
}

static void disconnect_peer(struct bt_conn *conn, uint16_t handle)
{
	assert_int_equal(bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN), 0);
	inject_disconn_complete(handle);
	assert_int_equal(osi_sem_take(&disconnected_sem, BT_SECONDS(1)), 0);
	bt_conn_unref(conn);
}

static void oldest_before(void *fixture)
{
	ARG_UNUSED(fixture);

	connected_count = 0;
	bt_keys_reset();
}

/* The suite must also pass when run without the other suites, so it
 * enables Bluetooth itself. settings_load() is needed for the "bt" tree
 * to be applied before the stack is ready.
 */
static void *oldest_setup(void)
{
	int err;

	err = bt_enable(NULL);
	assert_true(err == 0 || err == -EALREADY);
	assert_int_equal(settings_load(), 0);

	return NULL;
}



/* A full pool evicts the oldest entry and hands out its slot. */
static void bt_keys_oldest__test_evict_oldest_not_in_use(void **cm_state)
{
	(void)cm_state;
	const struct bt_keys *k1 = bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]);
	const struct bt_keys *k2 = bt_keys_get_addr(BT_ID_DEFAULT, &peers[1]);
	const struct bt_keys *k3;

	assert_non_null(k1);
	assert_non_null(k2);

	k3 = bt_keys_get_addr(BT_ID_DEFAULT, &peer_extra);
	assert_non_null(k3);
	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &peers[0]));
	assert_ptr_equal(k3, k1);
}

/* bt_keys_update_usage() makes an entry the most recently used one, so the
 * other entry becomes the eviction candidate.
 */
static void bt_keys_oldest__test_least_recently_used_is_evicted(void **cm_state)
{
	(void)cm_state;
	const struct bt_keys *k3;

	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]));
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[1]));

	bt_keys_update_usage(BT_ID_DEFAULT, &peers[0]);

	k3 = bt_keys_get_addr(BT_ID_DEFAULT, &peer_extra);
	assert_non_null(k3);
	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &peers[1]));
	assert_non_null(bt_keys_find_addr(BT_ID_DEFAULT, &peers[0]));
}

/* An entry in use by a connected peer must not be evicted. */
static void bt_keys_oldest__test_in_use_entry_is_not_evicted(void **cm_state)
{
	(void)cm_state;
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]));
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[1]));

	connect_peer(&peers[0], 0x0001);

	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peer_extra));
	assert_null(bt_keys_find_addr(BT_ID_DEFAULT, &peers[1]));
	assert_non_null(bt_keys_find_addr(BT_ID_DEFAULT, &peers[0]));

	disconnect_peer(connected_peers[0], 0x0001);
}

/* When every entry is in use no slot can be handed out. */
static void bt_keys_oldest__test_all_in_use_returns_null(void **cm_state)
{
	(void)cm_state;
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]));
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[1]));

	connect_peer(&peers[0], 0x0001);
	connect_peer(&peers[1], 0x0002);

	assert_null(bt_keys_get_addr(BT_ID_DEFAULT, &peer_extra));

	disconnect_peer(connected_peers[0], 0x0001);
	disconnect_peer(connected_peers[1], 0x0002);
}

/* CONFIG_BT_KEYS_SAVE_AGING_COUNTER_ON_PAIRING: bt_keys_update_usage()
 * writes the aging counter back to settings.
 */
static void bt_keys_oldest__test_aging_counter_is_stored(void **cm_state)
{
	(void)cm_state;
	static const bt_addr_le_t peer_aging = {
		.type = BT_ADDR_LE_PUBLIC,
		.a.val = {0x90, 0x91, 0x92, 0x93, 0x94, 0x95},
	};
	struct bt_keys *keys;
	uint32_t counter;
	char name[BT_SETTINGS_KEY_MAX];

	assert_non_null(bt_keys_get_type(BT_KEYS_IRK, BT_ID_DEFAULT, &peer_aging));
	assert_int_equal(bt_settings_encode_key(name, sizeof(name), "keys", &peer_aging, NULL), 0);

	/* A new key is already the most recently used one, so another key
	 * must be touched first for bt_keys_update_usage() to take effect.
	 */
	assert_non_null(bt_keys_get_addr(BT_ID_DEFAULT, &peers[0]));
	bt_keys_update_usage(BT_ID_DEFAULT, &peer_aging);

	keys = bt_keys_find_addr(BT_ID_DEFAULT, &peer_aging);
	assert_non_null(keys);
	counter = keys->aging_counter;

	/* A reload must restore the counter written by bt_keys_update_usage(). */
	bt_keys_reset();
	assert_int_equal(settings_load_subtree(name), 0);

	keys = bt_keys_find_addr(BT_ID_DEFAULT, &peer_aging);
	assert_non_null(keys);
	assert_int_equal(keys->aging_counter, counter);

	/* Drop the entry again: a leftover stored bond changes what the other
	 * suites restore from settings (e.g. bt_keys_commit's rl_entries count).
	 */
	assert_int_equal(settings_delete(name), 0);
}
#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

static void storage_init_58ac64f89f(void)
{
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	bt_test_require(osi_sem_init(&connected_sem, 0, 1));
#endif
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	bt_test_require(osi_sem_init(&disconnected_sem, 0, 1));
#endif
}

static void migrated_callbacks_init(void)
{
 static bool initialized;
 if (initialized) { return; }
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
 bt_test_require(bt_conn_cb_register(&bt_conn_cb_test_conn_cb));
#endif
 initialized = true;
}

#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
int bt_test_bt_keys_oldest_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	test_transport_init();
	storage_init_58ac64f89f();
	*state = oldest_setup();
	return 0;
}
static int bt_test_bt_keys_oldest_before(void **state)
{
	(void)state;
	oldest_before(*state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_keys_oldest[] = {
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	cmocka_unit_test_setup_teardown(bt_keys_oldest__test_evict_oldest_not_in_use, bt_test_bt_keys_oldest_before, NULL),
#endif
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	cmocka_unit_test_setup_teardown(bt_keys_oldest__test_least_recently_used_is_evicted, bt_test_bt_keys_oldest_before, NULL),
#endif
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	cmocka_unit_test_setup_teardown(bt_keys_oldest__test_in_use_entry_is_not_evicted, bt_test_bt_keys_oldest_before, NULL),
#endif
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	cmocka_unit_test_setup_teardown(bt_keys_oldest__test_all_in_use_returns_null, bt_test_bt_keys_oldest_before, NULL),
#endif
#if (defined(CONFIG_BT_KEYS_OVERWRITE_OLDEST))
	cmocka_unit_test_setup_teardown(bt_keys_oldest__test_aging_counter_is_stored, bt_test_bt_keys_oldest_before, NULL),
#endif
};
BT_TEST_SUITE(bt_keys_oldest, test_cases_bt_keys_oldest, bt_test_bt_keys_oldest_setup, NULL);
#endif


int bt_test_bt_keys_settings_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	test_transport_init();
	storage_init_58ac64f89f();
	*state = settings_suite_setup();
	return 0;
}
static int bt_test_bt_keys_settings_before(void **state)
{
	(void)state;
	reset_key_pool(*state);
	return 0;
}

int bt_test_bt_keys_settings_teardown(void **state)
{
	(void)state;
	settings_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_bt_keys_settings[] = {
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_stored_bond, bt_test_bt_keys_settings_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_delete_marker_removes_bond, bt_test_bt_keys_settings_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_old_format_bond, bt_test_bt_keys_settings_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_rejects_invalid_length, bt_test_bt_keys_settings_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_rejects_cfg_flags_mismatch, bt_test_bt_keys_settings_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_rejects_cfg_version_mismatch, bt_test_bt_keys_settings_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_settings__test_load_downgrades_unprovable_authentication, bt_test_bt_keys_settings_before, NULL),
};
BT_TEST_SUITE(bt_keys_settings, test_cases_bt_keys_settings, bt_test_bt_keys_settings_setup, bt_test_bt_keys_settings_teardown);

int bt_test_bt_keys_pool_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	test_transport_init();
	storage_init_58ac64f89f();
	*state = NULL;
	return 0;
}
static int bt_test_bt_keys_pool_before(void **state)
{
	(void)state;
	reset_key_pool(*state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_keys_pool[] = {
	cmocka_unit_test_setup_teardown(bt_keys_pool__test_get_addr_allocates_slot, bt_test_bt_keys_pool_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_pool__test_get_addr_returns_existing_when_full, bt_test_bt_keys_pool_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_pool__test_get_type_existing_and_full, bt_test_bt_keys_pool_before, NULL),
	cmocka_unit_test_setup_teardown(bt_keys_pool__test_find_and_foreach, bt_test_bt_keys_pool_before, NULL),
};
BT_TEST_SUITE(bt_keys_pool, test_cases_bt_keys_pool, bt_test_bt_keys_pool_setup, NULL);

int bt_test_bt_keys_commit_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	test_transport_init();
	storage_init_58ac64f89f();
	*state = keys_setup();
	return 0;
}



static const struct CMUnitTest test_cases_bt_keys_commit[] = {
	cmocka_unit_test(bt_keys_commit__test_settings_reload_does_not_add_irk_twice),
};
BT_TEST_SUITE(bt_keys_commit, test_cases_bt_keys_commit, bt_test_bt_keys_commit_setup, NULL);
