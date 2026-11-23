#include "support/test_support.h"
/* Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <embt/bluetooth/addr.h>
static const bt_addr_le_t example_addr = {
	.type = 0,
	.a = { { 0x66, 0x55, 0x44, 0x33, 0x22, 0x11 } },
};

static void bt_addr_le_to_str__test_public_uses_p_prefix(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_PUBLIC;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("P:11:22:33:44:55:66"));
	assert_string_equal(str, "P:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_random_uses_r_prefix(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_RANDOM;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("R:11:22:33:44:55:66"));
	assert_string_equal(str, "R:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_public_id_masks_to_public_prefix(void **cm_state)
{
	(void)cm_state;
	/* BT_ADDR_LE_PUBLIC_ID (0x02) is the identity-address marker derived from a
	 * public on-air address; the base type is still public, so it must print as P:.
	 */
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_PUBLIC_ID;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("P:11:22:33:44:55:66"));
	assert_string_equal(str, "P:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_random_id_masks_to_random_prefix(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_RANDOM_ID;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("R:11:22:33:44:55:66"));
	assert_string_equal(str, "R:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_hci_resolved_random_masks_to_random_prefix(void **cm_state)
{
	(void)cm_state;
	/* HCI events use bit 1 of Address_Type to indicate the Controller resolved an
	 * RPA. bt_addr_le_to_str() must mask that bit away so the public API string
	 * format only reflects the base address type. The extra bit is logged
	 * separately at the call sites that handle raw HCI event addresses.
	 */
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_RANDOM | 0x02U;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("R:11:22:33:44:55:66"));
	assert_string_equal(str, "R:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_hci_resolved_public_masks_to_public_prefix(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_PUBLIC | 0x02U;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("P:11:22:33:44:55:66"));
	assert_string_equal(str, "P:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_byte_order_is_big_endian_display(void **cm_state)
{
	(void)cm_state;
	/* a.val is stored little-endian (on-air order) but the string is rendered
	 * with the most significant byte first.
	 */
	bt_addr_le_t a = {
		.type = BT_ADDR_LE_PUBLIC,
		.a = { { 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 } },
	};
	char str[BT_ADDR_LE_STR_LEN];

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("P:01:02:03:04:05:06"));
	assert_string_equal(str, "P:01:02:03:04:05:06");
}

static void bt_addr_le_to_str__test_recommended_buffer_is_large_enough(void **cm_state)
{
	(void)cm_state;
	bt_addr_le_t a = {
		.type = BT_ADDR_LE_RANDOM,
		.a = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } },
	};
	char str[BT_ADDR_LE_STR_LEN] = { 0 };
	int written = bt_addr_le_to_str(&a, str, sizeof(str));

	/* snprintf returns the number of bytes it would have written, excluding the
	 * trailing NUL. The recommended buffer must fit the full string + NUL.
	 */
	assert_true(written > 0);
	assert_true((size_t)written < sizeof(str));
}

static void bt_addr_le_to_str__test_unresolved_formats_as_random(void **cm_state)
{
	(void)cm_state;
	/* BT_ADDR_LE_UNRESOLVED marks an RPA the Controller could not resolve. It
	 * is still a random address and must not be printed as a public one.
	 */
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_UNRESOLVED;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("R:11:22:33:44:55:66"));
	assert_string_equal(str, "R:11:22:33:44:55:66");
}

static void bt_addr_le_to_str__test_anonymous_formats_as_random(void **cm_state)
{
	(void)cm_state;
	/* BT_ADDR_LE_ANONYMOUS carries no address at all. The string format has no
	 * representation for that, so it is formatted as a random address; callers
	 * that need to convey the sentinel do so separately.
	 */
	bt_addr_le_t a = example_addr;
	char str[BT_ADDR_LE_STR_LEN];

	a.type = BT_ADDR_LE_ANONYMOUS;

	assert_int_equal(bt_addr_le_to_str(&a, str, sizeof(str)), strlen("R:11:22:33:44:55:66"));
	assert_string_equal(str, "R:11:22:33:44:55:66");
}





static const struct CMUnitTest test_cases_bt_addr_le_to_str[] = {
	cmocka_unit_test(bt_addr_le_to_str__test_public_uses_p_prefix),
	cmocka_unit_test(bt_addr_le_to_str__test_random_uses_r_prefix),
	cmocka_unit_test(bt_addr_le_to_str__test_public_id_masks_to_public_prefix),
	cmocka_unit_test(bt_addr_le_to_str__test_random_id_masks_to_random_prefix),
	cmocka_unit_test(bt_addr_le_to_str__test_hci_resolved_random_masks_to_random_prefix),
	cmocka_unit_test(bt_addr_le_to_str__test_hci_resolved_public_masks_to_public_prefix),
	cmocka_unit_test(bt_addr_le_to_str__test_byte_order_is_big_endian_display),
	cmocka_unit_test(bt_addr_le_to_str__test_recommended_buffer_is_large_enough),
	cmocka_unit_test(bt_addr_le_to_str__test_unresolved_formats_as_random),
	cmocka_unit_test(bt_addr_le_to_str__test_anonymous_formats_as_random),
};
BT_TEST_SUITE(bt_addr_le_to_str, test_cases_bt_addr_le_to_str, bt_test_host_setup, NULL);
