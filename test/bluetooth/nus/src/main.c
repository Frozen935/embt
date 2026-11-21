#include "support/test_support.h"
/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/services/nus.h>
BT_NUS_INST_DEFINE(test_nus);

static uint16_t nus_perm(const struct bt_uuid *uuid)
{
	for (size_t i = 0; i < test_nus.svc->attr_count; i++) {
		if (bt_uuid_cmp(test_nus.svc->attrs[i].uuid, uuid) == 0) {
			return test_nus.svc->attrs[i].perm;
		}
	}

	fail_msg("attribute not found");
	return 0;
}



static void nus_perm__test_ccc(void **cm_state)
{
	(void)cm_state;
	uint16_t expect = BT_GATT_PERM_READ;

	if (IS_ENABLED(CONFIG_BT_ZEPHYR_NUS_SECURITY_AUTHEN)) {
		expect |= BT_GATT_PERM_READ_AUTHEN | BT_GATT_PERM_WRITE_AUTHEN;
	} else if (IS_ENABLED(CONFIG_BT_ZEPHYR_NUS_SECURITY_ENCRYPT)) {
		expect |= BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT;
	} else {
		expect |= BT_GATT_PERM_WRITE;
	}

	assert_int_equal(nus_perm(BT_UUID_GATT_CCC), expect);
}

static void nus_perm__test_tx(void **cm_state)
{
	(void)cm_state;
	uint16_t expect = BT_GATT_PERM_NONE;

	if (IS_ENABLED(CONFIG_BT_ZEPHYR_NUS_SECURITY_AUTHEN)) {
		expect = BT_GATT_PERM_READ_AUTHEN;
	} else if (IS_ENABLED(CONFIG_BT_ZEPHYR_NUS_SECURITY_ENCRYPT)) {
		expect = BT_GATT_PERM_READ_ENCRYPT;
	}

	assert_int_equal(nus_perm(BT_UUID_NUS_TX_CHAR), expect);
}

static void nus_perm__test_rx(void **cm_state)
{
	(void)cm_state;
	uint16_t expect = BT_GATT_PERM_WRITE;

	if (IS_ENABLED(CONFIG_BT_ZEPHYR_NUS_SECURITY_AUTHEN)) {
		expect = BT_GATT_PERM_WRITE_AUTHEN;
	} else if (IS_ENABLED(CONFIG_BT_ZEPHYR_NUS_SECURITY_ENCRYPT)) {
		expect = BT_GATT_PERM_WRITE_ENCRYPT;
	}

	assert_int_equal(nus_perm(BT_UUID_NUS_RX_CHAR), expect);
}





static const struct CMUnitTest test_cases_nus_perm[] = {
	cmocka_unit_test(nus_perm__test_ccc),
	cmocka_unit_test(nus_perm__test_tx),
	cmocka_unit_test(nus_perm__test_rx),
};
BT_TEST_SUITE(nus_perm, test_cases_nus_perm, bt_test_host_setup, NULL);
