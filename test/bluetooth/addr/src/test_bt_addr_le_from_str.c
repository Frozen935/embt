#include "support/test_support.h"
/* Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
/* The example addresses used throughout these tests, in on-air (little-endian) order. */
static const bt_addr_t example = { { 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 } };
static const bt_addr_t example_alpha = { { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa } };

static void bt_addr_le_from_str__test_public(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("P:11:22:33:44:55:66", &a), 0);
	assert_int_equal(a.type, BT_ADDR_LE_PUBLIC);
	assert_true(bt_addr_eq(&a.a, &example));
}

static void bt_addr_le_from_str__test_random(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("R:11:22:33:44:55:66", &a), 0);
	assert_int_equal(a.type, BT_ADDR_LE_RANDOM);
	assert_true(bt_addr_eq(&a.a, &example));
}

static void bt_addr_le_from_str__test_prefix_and_digits_are_case_insensitive(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("p:11:22:33:44:55:66", &a), 0);
	assert_int_equal(a.type, BT_ADDR_LE_PUBLIC);
	assert_true(bt_addr_eq(&a.a, &example));

	assert_int_equal(bt_addr_le_from_str("r:aa:bb:cc:dd:ee:ff", &a), 0);
	assert_int_equal(a.type, BT_ADDR_LE_RANDOM);
	assert_true(bt_addr_eq(&a.a, &example_alpha));

	assert_int_equal(bt_addr_le_from_str("R:AA:BB:CC:DD:EE:FF", &a), 0);
	assert_int_equal(a.type, BT_ADDR_LE_RANDOM);
	assert_true(bt_addr_eq(&a.a, &example_alpha));
}

static void bt_addr_le_from_str__test_rejects_unknown_prefix(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("X:11:22:33:44:55:66", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("1:11:22:33:44:55:66", &a), -EINVAL);
}

static void bt_addr_le_from_str__test_rejects_missing_prefix(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	/* The bare address without a type prefix is not accepted. */
	assert_int_equal(bt_addr_le_from_str("11:22:33:44:55:66", &a), -EINVAL);
}

static void bt_addr_le_from_str__test_rejects_missing_separator(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("P11:22:33:44:55:66", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("R-11:22:33:44:55:66", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("R 11:22:33:44:55:66", &a), -EINVAL);
}

static void bt_addr_le_from_str__test_rejects_malformed_address(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("R:11:22:33:44:55", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("R:11:22:33:44:55:66:77", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("P:gg:22:33:44:55:66", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("P:11-22-33-44-55-66", &a), -EINVAL);
}

static void bt_addr_le_from_str__test_rejects_truncated_string(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;

	assert_int_equal(bt_addr_le_from_str("", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("P", &a), -EINVAL);
	assert_int_equal(bt_addr_le_from_str("P:", &a), -EINVAL);
}

static void bt_addr_le_from_str__test_roundtrip_through_to_str(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t src = {
		.type = BT_ADDR_LE_RANDOM,
		.a = { { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff } },
	};
	bt_addr_le_t dst;
	char str[BT_ADDR_LE_STR_LEN];
	int written;

	written = bt_addr_le_to_str(&src, str, sizeof(str));
	assert_int_equal(written, (int)strlen(str));
	assert_int_equal(bt_addr_le_from_str(str, &dst), 0);
	assert_true(bt_addr_le_eq(&src, &dst));
}





static const struct CMUnitTest test_cases_bt_addr_le_from_str[] = {
	cmocka_unit_test(bt_addr_le_from_str__test_public),
	cmocka_unit_test(bt_addr_le_from_str__test_random),
	cmocka_unit_test(bt_addr_le_from_str__test_prefix_and_digits_are_case_insensitive),
	cmocka_unit_test(bt_addr_le_from_str__test_rejects_unknown_prefix),
	cmocka_unit_test(bt_addr_le_from_str__test_rejects_missing_prefix),
	cmocka_unit_test(bt_addr_le_from_str__test_rejects_missing_separator),
	cmocka_unit_test(bt_addr_le_from_str__test_rejects_malformed_address),
	cmocka_unit_test(bt_addr_le_from_str__test_rejects_truncated_string),
	cmocka_unit_test(bt_addr_le_from_str__test_roundtrip_through_to_str),
};
BT_TEST_SUITE(bt_addr_le_from_str, test_cases_bt_addr_le_from_str, bt_test_host_setup, NULL);
