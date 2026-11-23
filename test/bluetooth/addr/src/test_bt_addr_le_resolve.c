#include "support/test_support.h"
/* Copyright (c) 2026 Demant A/S
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/bluetooth.h>
#include <common/rpa.h>
#include <host/keys.h>

#if defined(CONFIG_BT_SMP) && defined(CONFIG_BT_PRIVACY)
static void bt_addr_le_rpa_resolve_before(void *f)
{
	ARG_UNUSED(f);

	bt_keys_reset();
}



static void bt_addr_le_rpa_resolve__test_null_args(void **cm_state)
{
	(void)cm_state;
	const bt_addr_le_t addr = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = {{0x06, 0x05, 0x04, 0x03, 0x02, 0x01}},
	};
	bt_addr_le_t resolved;

	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, NULL, &resolved), -EINVAL);
	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, &addr, NULL), -EINVAL);
}

static void bt_addr_le_rpa_resolve__test_invalid_id_is_rejected(void **cm_state)
{
	(void)cm_state;
	const bt_addr_le_t input = {
		.type = BT_ADDR_LE_RANDOM,
		.a = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x46}},
	};
	bt_addr_le_t resolved;

	assert_true(bt_addr_le_is_rpa(&input));
	assert_int_equal(bt_addr_le_rpa_resolve(CONFIG_BT_ID_MAX, &input, &resolved), -EINVAL);
}

static void bt_addr_le_rpa_resolve__test_rpa_resolution_no_matching_irk(void **cm_state)
{
	(void)cm_state;
	const bt_addr_le_t input = {
		.type = BT_ADDR_LE_RANDOM,
		.a = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x46}},
	};
	bt_addr_le_t resolved;

	assert_true(bt_addr_le_is_rpa(&input));
	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, &input, &resolved), -ENOENT);
}

static void bt_addr_le_rpa_resolve__test_non_rpa_random_is_rejected(void **cm_state)
{
	(void)cm_state;
	const bt_addr_le_t input = {
		.type = BT_ADDR_LE_RANDOM,
		.a = {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
	};
	bt_addr_le_t resolved;

	assert_false(bt_addr_le_is_rpa(&input));
	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, &input, &resolved), -EINVAL);
}

static void bt_addr_le_rpa_resolve__test_identity_is_rejected(void **cm_state)
{
	(void)cm_state;
	const bt_addr_le_t input = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = {{0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6}},
	};
	bt_addr_le_t resolved;

	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, &input, &resolved), -EINVAL);
}

static void bt_addr_le_rpa_resolve__test_valid_irk_resolves_rpa_to_identity(void **cm_state)
{
	(void)cm_state;
	const uint8_t irk[16] = {
		0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
		0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
	};
	const bt_addr_le_t identity = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = {{0xc6, 0xc5, 0xc4, 0xc3, 0xc2, 0xc1}},
	};
	bt_addr_t rpa;
	bt_addr_le_t input = {
		.type = BT_ADDR_LE_RANDOM,
	};
	bt_addr_le_t resolved;
	struct bt_keys *keys;

	keys = bt_keys_get_addr(BT_ID_DEFAULT, &identity);
	assert_non_null(keys);

	bt_keys_add_type(keys, BT_KEYS_IRK);
	(void)memcpy(keys->irk.val, irk, sizeof(irk));

	assert_int_equal(bt_rpa_create(irk, &rpa), 0);
	bt_addr_copy(&input.a, &rpa);
	assert_true(bt_addr_le_is_rpa(&input));

	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, &input, &resolved), 0);
	assert_true(bt_addr_le_eq(&resolved, &identity));
}

static void bt_addr_le_rpa_resolve__test_rpa_for_different_identity_is_not_resolved(void **cm_state)
{
	(void)cm_state;
	const uint8_t irk[16] = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xf0, 0x01,
	};
	const bt_addr_le_t identity = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = {{0xd6, 0xd5, 0xd4, 0xd3, 0xd2, 0xd1}},
	};
	bt_addr_t rpa;
	bt_addr_le_t input = {
		.type = BT_ADDR_LE_RANDOM,
	};
	bt_addr_le_t resolved;
	struct bt_keys *keys;

	keys = bt_keys_get_addr(1, &identity);
	assert_non_null(keys);

	bt_keys_add_type(keys, BT_KEYS_IRK);
	(void)memcpy(keys->irk.val, irk, sizeof(irk));

	assert_int_equal(bt_rpa_create(irk, &rpa), 0);
	bt_addr_copy(&input.a, &rpa);
	assert_true(bt_addr_le_is_rpa(&input));

	assert_int_equal(bt_addr_le_rpa_resolve(BT_ID_DEFAULT, &input, &resolved), -ENOENT);
}


static int bt_test_bt_addr_le_rpa_resolve_before(void **state)
{
	(void)state;
	bt_addr_le_rpa_resolve_before(*state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_addr_le_rpa_resolve[] = {
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_null_args, bt_test_bt_addr_le_rpa_resolve_before, NULL),
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_invalid_id_is_rejected, bt_test_bt_addr_le_rpa_resolve_before, NULL),
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_rpa_resolution_no_matching_irk, bt_test_bt_addr_le_rpa_resolve_before, NULL),
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_non_rpa_random_is_rejected, bt_test_bt_addr_le_rpa_resolve_before, NULL),
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_identity_is_rejected, bt_test_bt_addr_le_rpa_resolve_before, NULL),
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_valid_irk_resolves_rpa_to_identity, bt_test_bt_addr_le_rpa_resolve_before, NULL),
	cmocka_unit_test_setup_teardown(bt_addr_le_rpa_resolve__test_rpa_for_different_identity_is_not_resolved, bt_test_bt_addr_le_rpa_resolve_before, NULL),
};
BT_TEST_SUITE(bt_addr_le_rpa_resolve, test_cases_bt_addr_le_rpa_resolve, bt_test_host_setup, NULL);

#endif /* CONFIG_BT_SMP && CONFIG_BT_PRIVACY */
