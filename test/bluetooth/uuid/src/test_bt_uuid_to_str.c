#include "support/test_support.h"
/* Copyright (c) 2022 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <string.h>
#include <embt/bluetooth/uuid.h>



static bool is_null_terminated(char *str, size_t size)
{
	return strnlen(str, size) < size;
}

static void result_is_null_terminated(const struct bt_uuid *uuid)
{
	char str[BT_UUID_STR_LEN];

	memset(str, 1, sizeof(str));
	bt_uuid_to_str(uuid, str, sizeof(str));
	assert_true(is_null_terminated(str, sizeof(str)));
}

static void result_str_is(const struct bt_uuid *uuid, const char *expected_str)
{
	char str[BT_UUID_STR_LEN] = {};

	bt_uuid_to_str(uuid, str, sizeof(str));
	do { if (!(is_null_terminated(str, sizeof(str)))) { skip(); } } while (0);
	assert_true((strcmp(str, expected_str) == 0));
}

static void bt_uuid_to_str__test_null_terminated_type_16(void **cm_state)
{
	(void)cm_state;
	result_is_null_terminated(BT_UUID_DECLARE_16(0));
}

static void bt_uuid_to_str__test_null_terminated_type_32(void **cm_state)
{
	(void)cm_state;
	result_is_null_terminated(BT_UUID_DECLARE_32(0));
}

static void bt_uuid_to_str__test_null_terminated_type_128(void **cm_state)
{
	(void)cm_state;
	result_is_null_terminated(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0l, 0, 0, 0, 0ll)));
}

static void bt_uuid_to_str__test_padding_type_16(void **cm_state)
{
	(void)cm_state;
	result_str_is(BT_UUID_DECLARE_16(0), "0000");
}

static void bt_uuid_to_str__test_padding_type_32(void **cm_state)
{
	(void)cm_state;
	result_str_is(BT_UUID_DECLARE_32(0), "00000000");
}

static void bt_uuid_to_str__test_padding_type_128(void **cm_state)
{
	(void)cm_state;
	result_str_is(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0l, 0, 0, 0, 0ll)),
		      "00000000-0000-0000-0000-000000000000");
}

static void bt_uuid_to_str__test_ordering_type_16(void **cm_state)
{
	(void)cm_state;
	result_str_is(BT_UUID_DECLARE_16(0xabcd), "abcd");
}


static void bt_uuid_to_str__test_ordering_type_32(void **cm_state)
{
	(void)cm_state;
	result_str_is(BT_UUID_DECLARE_32(0xabcdef12), "abcdef12");
}

static void bt_uuid_to_str__test_ordering_type_128(void **cm_state)
{
	(void)cm_state;
	result_str_is(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
		0xabcdef12, 0x3456, 0x9999, 0x9999, 0x999999999999)),
		"abcdef12-3456-9999-9999-999999999999");

	result_str_is(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
		0x99999999, 0x9999, 0xabcd, 0xef12, 0x999999999999)),
		"99999999-9999-abcd-ef12-999999999999");

	result_str_is(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
		0x99999999, 0x9999, 0x9999, 0x9999, 0xabcdef123456)),
		"99999999-9999-9999-9999-abcdef123456");
}





static const struct CMUnitTest test_cases_bt_uuid_to_str[] = {
	cmocka_unit_test(bt_uuid_to_str__test_null_terminated_type_16),
	cmocka_unit_test(bt_uuid_to_str__test_null_terminated_type_32),
	cmocka_unit_test(bt_uuid_to_str__test_null_terminated_type_128),
	cmocka_unit_test(bt_uuid_to_str__test_padding_type_16),
	cmocka_unit_test(bt_uuid_to_str__test_padding_type_32),
	cmocka_unit_test(bt_uuid_to_str__test_padding_type_128),
	cmocka_unit_test(bt_uuid_to_str__test_ordering_type_16),
	cmocka_unit_test(bt_uuid_to_str__test_ordering_type_32),
	cmocka_unit_test(bt_uuid_to_str__test_ordering_type_128),
};
BT_TEST_SUITE(bt_uuid_to_str, test_cases_bt_uuid_to_str, bt_test_host_setup, NULL);
