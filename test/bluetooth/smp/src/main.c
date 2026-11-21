#include "support/test_support.h"
/* main.c - Application main entry point */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stddef.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/conn.h>
#include "host/smp.h"



static void test_smp__test_bt_smp_err_to_str(void **cm_state)
{
	(void)cm_state;
	/* Test a couple of entries */
	assert_string_equal(bt_smp_err_to_str(0x00), "BT_SMP_ERR_SUCCESS");
	assert_string_equal(bt_smp_err_to_str(0x0a), "BT_SMP_ERR_INVALID_PARAMS");
	assert_string_equal(bt_smp_err_to_str(0x0F), "BT_SMP_ERR_KEY_REJECTED");

	/* Test entries that are not used */
	assert_memory_equal(bt_smp_err_to_str(0x10), "(unknown)", strlen("(unknown)"));
	assert_memory_equal(bt_smp_err_to_str(0xFF), "(unknown)", strlen("(unknown)"));

	for (uint16_t i = 0; i <= UINT8_MAX; i++) {
		assert_non_null(bt_smp_err_to_str(i));
	}
}

static void test_smp__test_bt_security_err_to_str(void **cm_state)
{
	(void)cm_state;
	/* Test a couple of entries */
	assert_string_equal(bt_security_err_to_str(BT_SECURITY_ERR_AUTH_FAIL), "BT_SECURITY_ERR_AUTH_FAIL");
	assert_string_equal(bt_security_err_to_str(BT_SECURITY_ERR_KEY_REJECTED), "BT_SECURITY_ERR_KEY_REJECTED");
	assert_string_equal(bt_security_err_to_str(BT_SECURITY_ERR_UNSPECIFIED), "BT_SECURITY_ERR_UNSPECIFIED");

	/* Test outside range */
	assert_string_equal(bt_security_err_to_str(BT_SECURITY_ERR_UNSPECIFIED + 1), "(unknown)");

	for (uint16_t i = 0; i <= UINT8_MAX; i++) {
		assert_non_null(bt_security_err_to_str(i));
	}
}





static const struct CMUnitTest test_cases_test_smp[] = {
	cmocka_unit_test(test_smp__test_bt_smp_err_to_str),
	cmocka_unit_test(test_smp__test_bt_security_err_to_str),
};
BT_TEST_SUITE(test_smp, test_cases_test_smp, bt_test_host_setup, NULL);
