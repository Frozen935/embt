#include "support/test_support.h"
/* Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/bluetooth/addr.h>
#include <host/addr_internal.h>



static const bt_addr_t example = { { 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 } };

static void bt_addr_le_is_resolved__test_identity_types_are_resolved(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = { .a = example };

	a.type = BT_ADDR_LE_PUBLIC_ID;
	assert_true(bt_addr_le_is_resolved(&a));

	a.type = BT_ADDR_LE_RANDOM_ID;
	assert_true(bt_addr_le_is_resolved(&a));
}

static void bt_addr_le_is_resolved__test_plain_types_are_not_resolved(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = { .a = example };

	a.type = BT_ADDR_LE_PUBLIC;
	assert_false(bt_addr_le_is_resolved(&a));

	a.type = BT_ADDR_LE_RANDOM;
	assert_false(bt_addr_le_is_resolved(&a));
}

static void bt_addr_le_is_resolved__test_sentinels_are_not_resolved(void **cm_state)
{
	(void)cm_state;
	/* The LE Extended Advertising Report sentinels have bit 1 set as well,
	 * but neither is a Controller-resolved identity address.
	 */
	bt_addr_le_t a = { .a = example };

	a.type = BT_ADDR_LE_UNRESOLVED;
	assert_false(bt_addr_le_is_resolved(&a));

	a.type = BT_ADDR_LE_ANONYMOUS;
	assert_false(bt_addr_le_is_resolved(&a));
}

static void bt_addr_le_is_resolved__test_copy_resolved_yields_base_type(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t src = { .a = example };
	bt_addr_le_t dst;

	src.type = BT_ADDR_LE_PUBLIC_ID;
	bt_addr_le_copy_resolved(&dst, &src);
	assert_int_equal(dst.type, BT_ADDR_LE_PUBLIC);
	assert_true(bt_addr_eq(&dst.a, &example));

	src.type = BT_ADDR_LE_RANDOM_ID;
	bt_addr_le_copy_resolved(&dst, &src);
	assert_int_equal(dst.type, BT_ADDR_LE_RANDOM);
	assert_true(bt_addr_eq(&dst.a, &example));
}





static const struct CMUnitTest test_cases_bt_addr_le_is_resolved[] = {
	cmocka_unit_test(bt_addr_le_is_resolved__test_identity_types_are_resolved),
	cmocka_unit_test(bt_addr_le_is_resolved__test_plain_types_are_not_resolved),
	cmocka_unit_test(bt_addr_le_is_resolved__test_sentinels_are_not_resolved),
	cmocka_unit_test(bt_addr_le_is_resolved__test_copy_resolved_yields_base_type),
};
BT_TEST_SUITE(bt_addr_le_is_resolved, test_cases_bt_addr_le_is_resolved, bt_test_host_setup, NULL);
