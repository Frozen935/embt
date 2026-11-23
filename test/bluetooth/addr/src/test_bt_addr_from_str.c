#include "support/test_support.h"
/* Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/bluetooth/addr.h>
static void bt_addr_from_str__test_reject_empty_string(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_missing_octet(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab:ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_empty_octet(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab::ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_short_octet(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab:b:ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_trailing_colon(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab:ab:ab:";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_octet_colon_a(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab:a::ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_octet_colon_b(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab::b:ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_octet_space_a(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab: b:ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_octet_space_b(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "ab:ab:ab:ab:a :ab";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_extra_space_before(void **cm_state)
{
	(void)cm_state;
	char *addr_str = " 00:00:00:00:00:00";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_extra_space_after(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "00:00:00:00:00:00 ";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_replace_space_first(void **cm_state)
{
	(void)cm_state;
	char *addr_str = " 0:00:00:00:00:00";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_replace_colon_first(void **cm_state)
{
	(void)cm_state;
	char *addr_str = ":0:00:00:00:00:00";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_non_hex(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "00:00:00:00:g0:00";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_reject_bad_colon(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "00.00:00:00:00:00";
	bt_addr_t a;

	assert_int_equal(bt_addr_from_str(addr_str, &a), -EINVAL);
}

static void bt_addr_from_str__test_order(void **cm_state)
{
	(void)cm_state;
	char *addr_str = "01:02:03:04:05:06";
	bt_addr_t a;
	bt_addr_t b = {{6, 5, 4, 3, 2, 1}};

	assert_int_equal(bt_addr_from_str(addr_str, &a), 0);
	assert_true(bt_addr_eq(&a, &b));
}

static void bt_addr_from_str__test_hex_case_equal(void **cm_state)
{
	(void)cm_state;
	char *addr_str_a = "ab:cd:ef:00:00:00";
	char *addr_str_b = "AB:CD:EF:00:00:00";
	bt_addr_t a;
	bt_addr_t b;

	assert_int_equal(bt_addr_from_str(addr_str_a, &a), 0);
	assert_int_equal(bt_addr_from_str(addr_str_b, &b), 0);
	assert_true(bt_addr_eq(&a, &b));
}

static void bt_addr_from_str__test_hex_case_not_equal(void **cm_state)
{
	(void)cm_state;
	char *addr_str_a = "aa:aa:aa:00:00:00";
	char *addr_str_b = "bb:bb:bb:00:00:00";
	bt_addr_t a;
	bt_addr_t b;

	assert_int_equal(bt_addr_from_str(addr_str_a, &a), 0);
	assert_int_equal(bt_addr_from_str(addr_str_b, &b), 0);
	assert_false(bt_addr_eq(&a, &b));
}





static const struct CMUnitTest test_cases_bt_addr_from_str[] = {
	cmocka_unit_test(bt_addr_from_str__test_reject_empty_string),
	cmocka_unit_test(bt_addr_from_str__test_reject_missing_octet),
	cmocka_unit_test(bt_addr_from_str__test_reject_empty_octet),
	cmocka_unit_test(bt_addr_from_str__test_reject_short_octet),
	cmocka_unit_test(bt_addr_from_str__test_reject_trailing_colon),
	cmocka_unit_test(bt_addr_from_str__test_reject_octet_colon_a),
	cmocka_unit_test(bt_addr_from_str__test_reject_octet_colon_b),
	cmocka_unit_test(bt_addr_from_str__test_reject_octet_space_a),
	cmocka_unit_test(bt_addr_from_str__test_reject_octet_space_b),
	cmocka_unit_test(bt_addr_from_str__test_reject_extra_space_before),
	cmocka_unit_test(bt_addr_from_str__test_reject_extra_space_after),
	cmocka_unit_test(bt_addr_from_str__test_reject_replace_space_first),
	cmocka_unit_test(bt_addr_from_str__test_reject_replace_colon_first),
	cmocka_unit_test(bt_addr_from_str__test_reject_non_hex),
	cmocka_unit_test(bt_addr_from_str__test_reject_bad_colon),
	cmocka_unit_test(bt_addr_from_str__test_order),
	cmocka_unit_test(bt_addr_from_str__test_hex_case_equal),
	cmocka_unit_test(bt_addr_from_str__test_hex_case_not_equal),
};
BT_TEST_SUITE(bt_addr_from_str, test_cases_bt_addr_from_str, bt_test_host_setup, NULL);
