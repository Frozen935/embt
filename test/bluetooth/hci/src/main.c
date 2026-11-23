#include "support/test_support.h"
/* main.c - Application main entry point */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stddef.h>
#include <embt/bluetooth/hci.h>



static void test_hci__test_bt_hci_err_to_str(void **cm_state)
{
	(void)cm_state;
	/* Test a couple of entries */
	assert_memory_equal(bt_hci_err_to_str(BT_HCI_ERR_CONN_TIMEOUT), "BT_HCI_ERR_CONN_TIMEOUT", strlen("BT_HCI_ERR_CONN_TIMEOUT"));
	assert_memory_equal(bt_hci_err_to_str(BT_HCI_ERR_REMOTE_USER_TERM_CONN), "BT_HCI_ERR_REMOTE_USER_TERM_CONN", strlen("BT_HCI_ERR_REMOTE_USER_TERM_CONN"));
	assert_memory_equal(bt_hci_err_to_str(BT_HCI_ERR_TOO_EARLY), "BT_HCI_ERR_TOO_EARLY", strlen("BT_HCI_ERR_TOO_EARLY"));

	/* Test a entries that is not used */
	assert_memory_equal(bt_hci_err_to_str(0x2b), "(unknown)", strlen("(unknown)"));
	assert_memory_equal(bt_hci_err_to_str(0xFF), "(unknown)", strlen("(unknown)"));

	for (uint16_t i = 0; i <= UINT8_MAX; i++) {
		assert_non_null(bt_hci_err_to_str(i));
	}
}





static const struct CMUnitTest test_cases_test_hci[] = {
	cmocka_unit_test(test_hci__test_bt_hci_err_to_str),
};
BT_TEST_SUITE(test_hci, test_cases_test_hci, bt_test_host_setup, NULL);
