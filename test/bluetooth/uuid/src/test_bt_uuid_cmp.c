#include "support/test_support.h"
/* Copyright (c) 2019 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <embt/bluetooth/uuid.h>

static struct bt_uuid_16 uuid_16 = BT_UUID_INIT_16(0xffff);

static struct bt_uuid_128 uuid_128 = BT_UUID_INIT_128(
	0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
	0x00, 0x10, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00);



static void bt_uuid_cmp__test_uuid_cmp(void **cm_state)
{
	(void)cm_state;
	/* Compare UUID 16 bits */
	assert_false(bt_uuid_cmp(&uuid_16.uuid, BT_UUID_DECLARE_16(0xffff)));

	/* Compare UUID 128 bits */
	assert_false(bt_uuid_cmp(&uuid_128.uuid, BT_UUID_DECLARE_16(0xffff)));

	/* Compare UUID 16 bits with UUID 128 bits */
	assert_false(bt_uuid_cmp(&uuid_16.uuid, &uuid_128.uuid));

	/* Compare different UUID 16 bits */
	assert_true(bt_uuid_cmp(&uuid_16.uuid, BT_UUID_DECLARE_16(0x0000)));

	/* Compare different UUID 128 bits */
	assert_true(bt_uuid_cmp(&uuid_128.uuid, BT_UUID_DECLARE_16(0x000)));
}





static const struct CMUnitTest test_cases_bt_uuid_cmp[] = {
	cmocka_unit_test(bt_uuid_cmp__test_uuid_cmp),
};
BT_TEST_SUITE(bt_uuid_cmp, test_cases_bt_uuid_cmp, bt_test_host_setup, NULL);
