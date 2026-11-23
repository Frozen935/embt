#include "support/test_support.h"
/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "mocks/hci_core.h"
#include "mocks/hci_core_expects.h"

void expect_call_count_bt_hci_le_rand(int call_count, uint8_t args_history[])
{
	const char *func_name = "bt_hci_le_rand";

	assert_int_equal(bt_hci_le_rand_fake.call_count, call_count);

	for (size_t i = 0; i < call_count; i++) {
		assert_non_null(bt_hci_le_rand_fake.arg0_history[i]);
		assert_int_equal(bt_hci_le_rand_fake.arg1_history[i], args_history[i]);
	}
}

void expect_not_called_bt_hci_le_rand(void)
{
	const char *func_name = "bt_hci_le_rand";

	assert_int_equal(bt_hci_le_rand_fake.call_count, 0);
}
