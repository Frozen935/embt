#include "support/test_support.h"
/* Copyright (c) 2022 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/bluetooth/addr.h>
static void bt_addr_le_eq__test_all_zero(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {.type = 0, .a = {{0, 0, 0, 0, 0, 0}}};
	bt_addr_le_t b = a;

	assert_true(bt_addr_le_eq(&a, &b));
}

static void bt_addr_le_eq__test_type_not_zero(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {.type = 1, .a = {{1, 2, 3, 4, 5, 6}}};
	bt_addr_le_t b = a;

	assert_true(bt_addr_le_eq(&a, &b));
}

static void bt_addr_le_eq__test_type_matters(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {.type = 0, .a = {{1, 2, 3, 4, 5, 6}}};
	bt_addr_le_t b = a;

	do { if (!(bt_addr_le_eq(&a, &b))) { skip(); } } while (0);
	a.type = 1;
	assert_false(bt_addr_le_eq(&a, &b));
}

static void bt_addr_le_eq__test_address_matters_start(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {.type = 0, .a = {{1, 2, 3, 4, 5, 6}}};
	bt_addr_le_t b = a;

	do { if (!(bt_addr_le_eq(&a, &b))) { skip(); } } while (0);
	a.a.val[0] = 0;
	assert_false(bt_addr_le_eq(&a, &b));
}

static void bt_addr_le_eq__test_address_matters_end(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {.type = 0, .a = {{1, 2, 3, 4, 5, 6}}};
	bt_addr_le_t b = a;

	do { if (!(bt_addr_le_eq(&a, &b))) { skip(); } } while (0);
	a.a.val[5] = 0;
	assert_false(bt_addr_le_eq(&a, &b));
}

static void bt_addr_le_eq__test_only_type_and_address_matters(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a;
	bt_addr_le_t b;

	/* Make anything that is not the type and address unequal bytes. */
	memset(&a, 0xaa, sizeof(a));
	memset(&b, 0xbb, sizeof(b));
	a.type = 1;
	b.type = 1;
	memset(a.a.val, 1, sizeof(a.a.val));
	memset(b.a.val, 1, sizeof(b.a.val));

	assert_true(bt_addr_le_eq(&a, &b));
}

static void bt_addr_le_eq__test_same_object(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {.type = 0, .a = {{1, 2, 3, 4, 5, 6}}};

	assert_true(bt_addr_le_eq(&a, &a));
}





static const struct CMUnitTest test_cases_bt_addr_le_eq[] = {
	cmocka_unit_test(bt_addr_le_eq__test_all_zero),
	cmocka_unit_test(bt_addr_le_eq__test_type_not_zero),
	cmocka_unit_test(bt_addr_le_eq__test_type_matters),
	cmocka_unit_test(bt_addr_le_eq__test_address_matters_start),
	cmocka_unit_test(bt_addr_le_eq__test_address_matters_end),
	cmocka_unit_test(bt_addr_le_eq__test_only_type_and_address_matters),
	cmocka_unit_test(bt_addr_le_eq__test_same_object),
};
BT_TEST_SUITE(bt_addr_le_eq, test_cases_bt_addr_le_eq, bt_test_host_setup, NULL);
