#include "support/test_support.h"
#include <embt/base/log.h>
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <embt/net_buf.h>
#include <embt/bluetooth/mesh.h>
#include <stdlib.h>

#include "settings.h"
#include "brg_cfg.h"
#include "foundation.h"

#define TEST_VECT_SZ (CONFIG_BT_MESH_BRG_TABLE_ITEMS_MAX + 1)

static struct test_brg_cfg_row {
	uint8_t direction;
	uint16_t net_idx1;
	uint16_t net_idx2;
	uint16_t addr1;
	uint16_t addr2;
} test_vector[TEST_VECT_SZ];

#define ADDR1_BASE (1)
#define ADDR2_BASE (100)

/**** Helper functions ****/
static void setup(void *f)
{
	/* create test vector */
	for (int i = 0; i < TEST_VECT_SZ; i++) {
		test_vector[i].direction = i < (TEST_VECT_SZ / 2) ? 1 : 2;
		test_vector[i].net_idx1 = (i / 8);
		test_vector[i].addr1 = ADDR1_BASE + i;
		test_vector[i].net_idx2 = (i / 8) + 16;
		test_vector[i].addr2 = ADDR2_BASE + i;
	}
}

/**** Mocked functions ****/

void bt_mesh_settings_store_schedule(enum bt_mesh_settings_flag flag)
{
	check_expected(flag);
}

int settings_save_one(const char *name, const void *value, size_t val_len)
{
	check_expected(name);
	check_expected(val_len);
	check_expected(value);
	return 0;
}

int settings_delete(const char *name)
{
	check_expected(name);
	return 0;
}

struct bt_mesh_subnet *bt_mesh_subnet_get(uint16_t net_idx)
{
	/* Return anything non-zero. */
	return (struct bt_mesh_subnet *)1;
}

/**** Mocked functions - end ****/

static void check_fill_all_bt_entries(void)
{
	uint8_t status;
	int err;

	for (int i = 0; i < TEST_VECT_SZ; i++) {

		if (i < CONFIG_BT_MESH_BRG_TABLE_ITEMS_MAX) {
			expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		}

		err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction, test_vector[i].net_idx1,
					      test_vector[i].net_idx2, test_vector[i].addr1,
					      test_vector[i].addr2, &status);

		assert_int_equal(err, 0);

		if (i != CONFIG_BT_MESH_BRG_TABLE_ITEMS_MAX) {
			assert_int_equal(status, STATUS_SUCCESS);
		} else {
			assert_int_equal(status, STATUS_INSUFF_RESOURCES);
		}
	}
}

static void check_delete_all_bt_entries(void)
{
	uint8_t status;

	for (int i = 0; i < TEST_VECT_SZ; i++) {

		if (i < CONFIG_BT_MESH_BRG_TABLE_ITEMS_MAX) {
			expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		}

		int err = bt_mesh_brg_cfg_tbl_remove(test_vector[i].net_idx1,
						     test_vector[i].net_idx2, test_vector[i].addr1,
						     test_vector[i].addr2, &status);

		assert_int_equal(err, 0);
		assert_int_equal(status, STATUS_SUCCESS);
	}
}

static void check_bt_mesh_brg_cfg_tbl_reset(void)
{
	int err;

	expect_string(settings_delete, name, "bt/mesh/brg_en");
	expect_string(settings_delete, name, "bt/mesh/brg_tbl");
	err = bt_mesh_brg_cfg_tbl_reset();
	assert_int_equal(err, 0);
}

/**** Tests ****/



/* Test if basic functionality (add and remove entries) works correctly. */
static void bt_mesh_brg_cfg__test_basic_functionality_storage(void **cm_state)
{
	(void)cm_state;
	check_bt_mesh_brg_cfg_tbl_reset();

	/* Test add entries to bridging table. */
	check_fill_all_bt_entries();

	/* Test remove entries from bridging table, and then fill it again. */
	check_delete_all_bt_entries();
	check_fill_all_bt_entries();

	/* Test resetting of the table, and then fill it again. */
	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries();

	uint8_t status;
	int err;
	uint16_t net_idx1 = test_vector[TEST_VECT_SZ - 1].net_idx1;
	uint16_t net_idx2 = test_vector[TEST_VECT_SZ - 1].net_idx2;
	uint16_t addr1 = BT_MESH_ADDR_UNASSIGNED;

	/* Try removing entries with invalid params */
	uint16_t addr2 = BT_MESH_ADDR_ALL_NODES;

	err = bt_mesh_brg_cfg_tbl_remove(net_idx1, net_idx2, addr1, addr2, &status);
	assert_int_equal(err, -EINVAL);

	addr2 = BT_MESH_ADDR_UNASSIGNED;
	addr1 = BT_MESH_ADDR_RELAYS;
	err = bt_mesh_brg_cfg_tbl_remove(net_idx1, net_idx2, addr1, addr2, &status);
	assert_int_equal(err, -EINVAL);

	addr1 = BT_MESH_ADDR_UNASSIGNED;
	net_idx1 = 4096;
	err = bt_mesh_brg_cfg_tbl_remove(net_idx1, net_idx2, addr1, addr2, &status);
	assert_int_equal(err, -EINVAL);

	net_idx1 = test_vector[TEST_VECT_SZ - 1].net_idx1;
	net_idx2 = 4096;
	err = bt_mesh_brg_cfg_tbl_remove(net_idx1, net_idx2, addr1, addr2, &status);
	assert_int_equal(err, -EINVAL);

	/* Test remove entries matching netkey1, and netkey2 */
	net_idx2 = test_vector[TEST_VECT_SZ - 1].net_idx2;
	err = bt_mesh_brg_cfg_tbl_remove(net_idx1, net_idx2, addr1, addr2, &status);
	assert_int_equal(err, 0);

	const struct bt_mesh_brg_cfg_row *brg_tbl;
	int n = bt_mesh_brg_cfg_tbl_get(&brg_tbl);

	assert_true(n > 0);

	for (int i = 0; i < n; i++) {
		assert_true(brg_tbl[i].net_idx1 != net_idx1);
		assert_true(brg_tbl[i].net_idx2 != net_idx2);
	}

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries();

	/* Test remove entries matching netkey1, and netkey2, and addr1 */
	addr1 = test_vector[TEST_VECT_SZ - 1].addr1;
	n = bt_mesh_brg_cfg_tbl_get(&brg_tbl);

	assert_true(n > 0);

	for (int i = 0; i < n; i++) {
		assert_true(brg_tbl[i].net_idx1 != net_idx1);
		assert_true(brg_tbl[i].net_idx2 != net_idx2);
		assert_true(brg_tbl[i].addr1 != addr1);
	}

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries();

	/* Test remove entries matching netkey1, and netkey2, and addr2 */
	addr1 = BT_MESH_ADDR_UNASSIGNED;
	addr2 = test_vector[TEST_VECT_SZ - 1].addr2;
	n = bt_mesh_brg_cfg_tbl_get(&brg_tbl);

	assert_true(n > 0);

	for (int i = 0; i < n; i++) {
		assert_true(brg_tbl[i].net_idx1 != net_idx1);
		assert_true(brg_tbl[i].net_idx2 != net_idx2);
		assert_true(brg_tbl[i].addr2 != addr2);
	}
}

static void check_bt_mesh_brg_cfg_tbl_multiple_delete(int expect_left)
{
	uint8_t status;
	int err;
	int n;
	const struct bt_mesh_brg_cfg_row *brg_tbl;

	n = bt_mesh_brg_cfg_tbl_get(&brg_tbl);
	assert_int_equal(n, TEST_VECT_SZ - 1);

	expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
	err = bt_mesh_brg_cfg_tbl_remove(test_vector[1].net_idx1, test_vector[1].net_idx2,
					 test_vector[1].addr1, BT_MESH_ADDR_UNASSIGNED, &status);
	assert_int_equal(err, 0);

	n = bt_mesh_brg_cfg_tbl_get(&brg_tbl);
	assert_int_equal(n, expect_left);

	for (int i = 0; i < n; i++) {
		assert_true(brg_tbl[i].net_idx1 == test_vector[0].net_idx1);
		assert_true(brg_tbl[i].net_idx2 == test_vector[0].net_idx2);
		assert_true(brg_tbl[i].addr1 == test_vector[0].addr1);
		assert_true(brg_tbl[i].addr2 == test_vector[i * 2].addr2);
	}
}

static void bt_mesh_brg_cfg__test_removal_multiple_entries(void **cm_state)
{
	(void)cm_state;
	check_bt_mesh_brg_cfg_tbl_reset();

	uint8_t status;
	int err;

	/* Test removal of every second entry */
	for (int i = 0; i < TEST_VECT_SZ - 1; i++) {
		expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction, test_vector[i % 2].net_idx1,
					      test_vector[i % 2].net_idx2, test_vector[i % 2].addr1,
					      test_vector[i].addr2, &status);
		assert_int_equal(err, 0);
		assert_int_equal(status, STATUS_SUCCESS);
	}

	check_bt_mesh_brg_cfg_tbl_multiple_delete((TEST_VECT_SZ - 1) / 2);
	check_bt_mesh_brg_cfg_tbl_reset();

	/* Test removal of all entries, except first */
	for (int i = 0; i < TEST_VECT_SZ - 1; i++) {
		expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		if (i == 0) {
			err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction,
						      test_vector[i].net_idx1,
						      test_vector[i].net_idx2, test_vector[i].addr1,
						      test_vector[i].addr2, &status);
		} else {
			err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction,
						      test_vector[1].net_idx1,
						      test_vector[1].net_idx2, test_vector[1].addr1,
						      test_vector[i].addr2, &status);
		}
		assert_int_equal(err, 0);
		assert_int_equal(status, STATUS_SUCCESS);
	}

	check_bt_mesh_brg_cfg_tbl_multiple_delete(1);
	check_bt_mesh_brg_cfg_tbl_reset();

	/* Test removal of all entries, except last */
	for (int i = TEST_VECT_SZ - 2; i >= 0; i--) {
		expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		if (i == 0) {
			err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction,
						      test_vector[i].net_idx1,
						      test_vector[i].net_idx2, test_vector[i].addr1,
						      test_vector[i].addr2, &status);
		} else {
			err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction,
						      test_vector[1].net_idx1,
						      test_vector[1].net_idx2, test_vector[1].addr1,
						      test_vector[i].addr2, &status);
		}
		assert_int_equal(err, 0);
		assert_int_equal(status, STATUS_SUCCESS);
	}

	check_bt_mesh_brg_cfg_tbl_multiple_delete(1);
	check_bt_mesh_brg_cfg_tbl_reset();

	/* Test removal of all entries */
	for (int i = 0; i < TEST_VECT_SZ - 1; i++) {
		expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction, test_vector[1].net_idx1,
					      test_vector[1].net_idx2, test_vector[1].addr1,
					      test_vector[i].addr2, &status);
		assert_int_equal(err, 0);
		assert_int_equal(status, STATUS_SUCCESS);
	}

	check_bt_mesh_brg_cfg_tbl_multiple_delete(0);
}

static void pending_store_enable_create_expectations(bool *enable_val)
{
	if (*enable_val) {
		expect_string(settings_save_one, name, "bt/mesh/brg_en");
		expect_value(settings_save_one, val_len, 1);
		expect_memory(settings_save_one, value, enable_val, sizeof(*enable_val));
	} else {
		expect_string(settings_delete, name, "bt/mesh/brg_en");
	}
}

static void pending_store_tbl_create_expectations(int n, const struct bt_mesh_brg_cfg_row *tbl_val)
{
	if (n > 0) {
		expect_string(settings_save_one, name, "bt/mesh/brg_tbl");
		expect_value(settings_save_one, val_len, n * sizeof(struct bt_mesh_brg_cfg_row));
		expect_memory(settings_save_one, value, tbl_val, n * sizeof(*tbl_val));
	} else {
		expect_string(settings_delete, name, "bt/mesh/brg_tbl");
	}
}

/* Test if enable flag is stored correctly. */
static void bt_mesh_brg_cfg__test_brg_cfg_en(void **cm_state)
{
	(void)cm_state;
	int err;
	bool val;

	check_bt_mesh_brg_cfg_tbl_reset();
	val = bt_mesh_brg_cfg_enable_get();
	assert_int_equal(val, false);
	/* No changed to the states, nothing to check. */

	expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
	err = bt_mesh_brg_cfg_enable_set(true);
	assert_int_equal(err, 0);
	val = bt_mesh_brg_cfg_enable_get();
	pending_store_enable_create_expectations(&val);
	bt_mesh_brg_cfg_pending_store();

	assert_int_equal(bt_mesh_brg_cfg_enable_get(), true);
}

/* Test if pending store works correctly by adding one entry to the table. */
static void bt_mesh_brg_cfg__test_brg_tbl_pending_store(void **cm_state)
{
	(void)cm_state;
	uint8_t status;
	int n, err;
	struct bt_mesh_brg_cfg_row test_vec = {
		.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		.net_idx1 = 1,
		.net_idx2 = 2,
		.addr1 = 3,
		.addr2 = 4,
	};

	check_bt_mesh_brg_cfg_tbl_reset();
	expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
	err = bt_mesh_brg_cfg_tbl_add(test_vec.direction, test_vec.net_idx1, test_vec.net_idx2,
				      test_vec.addr1, test_vec.addr2, &status);
	assert_int_equal(err, 0);
	assert_int_equal(status, STATUS_SUCCESS);

	const struct bt_mesh_brg_cfg_row *tbl;

	n = bt_mesh_brg_cfg_tbl_get(&tbl);

	assert_int_equal(n, 1);
	assert_true(tbl);

	pending_store_tbl_create_expectations(1, &test_vec);
	bt_mesh_brg_cfg_pending_store();
}

/* Value is prohibited. */
#define BT_MESH_BRG_CFG_DIR_PROHIBITED 0
/* Values above and including this is prohibited. */
#define BT_MESH_BRG_CFG_DIR_MAX 3

/* Test if invalid entries are not added to the table. */
static void bt_mesh_brg_cfg__test_tbl_add_invalid_ip(void **cm_state)
{
	(void)cm_state;
	uint8_t status;
	int err;
	/* Create test vector array of test_brg_cfg_row iteams with invalid values.
	 * Each vector has only one invalid field value, rest all are valid values.
	 */
	const struct test_brg_cfg_row inv_test_vector[] = {
		/* Direction has invalid values */
		{.direction = BT_MESH_BRG_CFG_DIR_PROHIBITED,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 2},
		{.direction = BT_MESH_BRG_CFG_DIR_MAX,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 2},
		/* Out of range netidx values */
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 4096,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 2},
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 4096,
		 .addr1 = 1,
		 .addr2 = 2},
		/* Same netidx values */
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 0,
		 .addr1 = 1,
		 .addr2 = 2},
		/* Same addr values */
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 1},
		/* Invalid address1 value */
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 0,
		 .addr2 = 1},
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 0x8000,
		 .addr2 = 1},
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 0xC000,
		 .addr2 = 1},
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 0xFFFE,
		 .addr2 = 1},
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 0xFFFF,
		 .addr2 = 1},
		/* Invalid address2 values */
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 0},
		{.direction = BT_MESH_BRG_CFG_DIR_ONEWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 0xFFFF},
		{.direction = BT_MESH_BRG_CFG_DIR_TWOWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 0x8000},
		{.direction = BT_MESH_BRG_CFG_DIR_TWOWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 0xC000},
		{.direction = BT_MESH_BRG_CFG_DIR_TWOWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 0xFFFE},
		{.direction = BT_MESH_BRG_CFG_DIR_TWOWAY,
		 .net_idx1 = 0,
		 .net_idx2 = 1,
		 .addr1 = 1,
		 .addr2 = 0xFFFF},
	};

	check_bt_mesh_brg_cfg_tbl_reset();

	for (int i = 0; i < ARRAY_SIZE(inv_test_vector); i++) {
		err = bt_mesh_brg_cfg_tbl_add(inv_test_vector[i].direction,
					      inv_test_vector[i].net_idx1,
					      inv_test_vector[i].net_idx2, inv_test_vector[i].addr1,
					      inv_test_vector[i].addr2, &status);
		assert_int_equal(err, -EINVAL);
	}
}

/* Following are helper functions for the test that checks the iteration logic */
#define NUM_MSGS (10000)

static void print_brg_tbl(void)
{
	const struct bt_mesh_brg_cfg_row *tbl;
	int n = bt_mesh_brg_cfg_tbl_get(&tbl);

	assert_true(n <= CONFIG_BT_MESH_BRG_TABLE_ITEMS_MAX);

	for (int i = 0; i < n; i++) {
		bt_log_print("entry: %3d # dir: %d, net_idx1: %3d, addr1: %3d, net_idx2: %3d, addr2: "
		       "%3d\n",
		       i, tbl[i].direction, tbl[i].net_idx1, tbl[i].addr1, tbl[i].net_idx2,
		       tbl[i].addr2);
	}
}

static void check_fill_all_bt_entries_reversed(void)
{
	uint8_t status;
	int err;

	for (int i = TEST_VECT_SZ - 2; i >= 0; i--) {
		expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		err = bt_mesh_brg_cfg_tbl_add(test_vector[i].direction, test_vector[i].net_idx1,
					      test_vector[i].net_idx2, test_vector[i].addr1,
					      test_vector[i].addr2, &status);
		assert_int_equal(err, 0);
	}

	int last = TEST_VECT_SZ - 1;

	err = bt_mesh_brg_cfg_tbl_add(test_vector[last].direction, test_vector[last].net_idx1,
				      test_vector[last].net_idx2, test_vector[last].addr1,
				      test_vector[last].addr2, &status);
	assert_int_equal(err, 0);
	assert_int_equal(status, STATUS_INSUFF_RESOURCES);
}

static struct test_brg_cfg_row test_vector_copy[TEST_VECT_SZ - 1];

static void check_fill_all_bt_entries_randomly(void)
{
	uint8_t status;
	int err;
	int copy_cnt = ARRAY_SIZE(test_vector_copy);

	memcpy(test_vector_copy, test_vector, sizeof(test_vector_copy));

	for (int i = 0; i < copy_cnt; i++) {
		int idx = rand() % copy_cnt;
		struct test_brg_cfg_row tmp = test_vector_copy[i];

		test_vector_copy[i] = test_vector_copy[idx];
		test_vector_copy[idx] = tmp;
	}

	for (int i = 0; i < copy_cnt; i++) {
		expect_value(bt_mesh_settings_store_schedule, flag, BT_MESH_SETTINGS_BRG_PENDING);
		err = bt_mesh_brg_cfg_tbl_add(
			test_vector_copy[i].direction, test_vector_copy[i].net_idx1,
			test_vector_copy[i].net_idx2, test_vector_copy[i].addr1,
			test_vector_copy[i].addr2, &status);
		assert_int_equal(err, 0);
		assert_int_equal(status, STATUS_SUCCESS);
	}

	int last = TEST_VECT_SZ - 1;

	err = bt_mesh_brg_cfg_tbl_add(test_vector[last].direction, test_vector[last].net_idx1,
				      test_vector[last].net_idx2, test_vector[last].addr1,
				      test_vector[last].addr2, &status);
	assert_int_equal(err, 0);
	assert_int_equal(status, STATUS_INSUFF_RESOURCES);
}

static void subnet_relay_cb_check(uint16_t new_net_idx, void *user_data)
{
	int idx = *(int *)user_data;

	assert_int_equal(new_net_idx, test_vector[idx].net_idx2);
}

static void subnet_relay_cb_check_rev(uint16_t new_net_idx, void *user_data)
{
	int idx = *(int *)user_data;

	if (test_vector[idx].direction == 2) {
		assert_int_equal(new_net_idx, test_vector[idx].net_idx1);
	} else {
		/* Should never assert. Test vector created in setup(). */
		assert_true(false);
	}
}

static void test_bridging_performance(bool test_one_way)
{
	int idx;
	uint64_t start_us;
	uint64_t elapsed_us = 0;

	for (int i = 0; i < NUM_MSGS; i++) {
		/* randomly pick an entry from the test vector */
		idx = rand() % TEST_VECT_SZ;

		/* check src to dst bridging*/
		const struct bt_mesh_brg_cfg_row *tbl_row = NULL;

		start_us = bt_test_uptime_us();
		bt_mesh_brg_cfg_tbl_foreach_subnet(test_vector[idx].addr1, test_vector[idx].addr2,
						   test_vector[idx].net_idx1, subnet_relay_cb_check,
						   &idx);
		elapsed_us += bt_test_uptime_us() - start_us;

		if (test_one_way) {
			continue;
		}

		/* check dst to src bridging - for the same test vector src-dst pairs
		 * but now, reverse them and consider packets are arriving on net_idx2
		 */
		tbl_row = NULL;
		start_us = bt_test_uptime_us();
		bt_mesh_brg_cfg_tbl_foreach_subnet(test_vector[idx].addr2, test_vector[idx].addr1,
						   test_vector[idx].net_idx2,
						   subnet_relay_cb_check_rev, &idx);
		elapsed_us += bt_test_uptime_us() - start_us;
	}
	bt_log_print("elapsed_us: %llu\n", (unsigned long long)elapsed_us);
}

/* Test checks iteration logic and performance when run on real devices. */
static void bt_mesh_brg_cfg__test_zcheck_entry_randomly_sorting(void **cm_state)
{
	(void)cm_state;
	bt_log_print("num msgs: %d\n\n", NUM_MSGS);

	/* Test performance when packets are flowing in one directions */
	/* Fill bridging table in sorted order */
	bt_log_print("\n\nPackets going only in one direction (from outside towards the subnet)\n");
	bt_log_print("\nBridging table is pre-filled in sorted order\n");

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries();
	print_brg_tbl();
	test_bridging_performance(true);

	/* Fill bridging table in reversed order */
	bt_log_print("\nBridging table is pre-filled in reversed order\n");

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries_reversed();
	print_brg_tbl();
	test_bridging_performance(true);

	/* Fill bridging table in random order */
	bt_log_print("\nBridging table is pre-filled in random order\n");

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries_randomly();
	print_brg_tbl();
	test_bridging_performance(true);

	/* Test performance when packets are flowing in both directions - use same dataset. */
	bt_log_print("\n\nPackets going in both directions (same data set, flip src and dst pairs)\n");
	bt_log_print("\nBridging table is pre-filled in sorted order\n");

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries();
	print_brg_tbl();
	test_bridging_performance(false);

	/* Fill bridging table in reversed order */
	bt_log_print("\nBridging table is pre-filled in reversed order\n");

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries_reversed();
	print_brg_tbl();
	test_bridging_performance(false);

	/* Fill bridging table in random order */
	bt_log_print("\nBridging table is pre-filled in random order\n");

	check_bt_mesh_brg_cfg_tbl_reset();
	check_fill_all_bt_entries_randomly();
	print_brg_tbl();
	test_bridging_performance(false);
}


static int bt_test_bt_mesh_brg_cfg_before(void **state)
{
	(void)state;
	setup(*state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_mesh_brg_cfg[] = {
	cmocka_unit_test_setup_teardown(bt_mesh_brg_cfg__test_basic_functionality_storage, bt_test_bt_mesh_brg_cfg_before, NULL),
	cmocka_unit_test_setup_teardown(bt_mesh_brg_cfg__test_removal_multiple_entries, bt_test_bt_mesh_brg_cfg_before, NULL),
	cmocka_unit_test_setup_teardown(bt_mesh_brg_cfg__test_brg_cfg_en, bt_test_bt_mesh_brg_cfg_before, NULL),
	cmocka_unit_test_setup_teardown(bt_mesh_brg_cfg__test_brg_tbl_pending_store, bt_test_bt_mesh_brg_cfg_before, NULL),
	cmocka_unit_test_setup_teardown(bt_mesh_brg_cfg__test_tbl_add_invalid_ip, bt_test_bt_mesh_brg_cfg_before, NULL),
	cmocka_unit_test_setup_teardown(bt_mesh_brg_cfg__test_zcheck_entry_randomly_sorting, bt_test_bt_mesh_brg_cfg_before, NULL),
};
BT_TEST_SUITE(bt_mesh_brg_cfg, test_cases_bt_mesh_brg_cfg, bt_test_base_setup, NULL);
