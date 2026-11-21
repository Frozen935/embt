#include "support/test_support.h"
/* Copyright (c) 2019 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <embt/bluetooth/uuid.h>

static struct bt_uuid_128 le_128 = BT_UUID_INIT_128(
	0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
	0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00);



static void bt_uuid_create__test_uuid_create(void **cm_state)
{
	(void)cm_state;
	uint8_t le16[] = { 0x01, 0x00 };
	uint8_t be16[] = { 0x00, 0x01 };
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	/* Create UUID from LE 16 bit byte array */
	assert_true(bt_uuid_create(&u.uuid, le16, sizeof(le16)));

	/* Compare UUID 16 bits */
	assert_true(bt_uuid_cmp(&u.uuid, BT_UUID_DECLARE_16(0x0001)) == 0);

	/* Compare UUID 128 bits */
	assert_true(bt_uuid_cmp(&u.uuid, &le_128.uuid) == 0);

	/* Compare swapped UUID 16 bits */
	assert_false(bt_uuid_cmp(&u.uuid, BT_UUID_DECLARE_16(0x0100)) == 0);

	/* Create UUID from BE 16 bit byte array */
	assert_true(bt_uuid_create(&u.uuid, be16, sizeof(be16)));

	/* Compare UUID 16 bits */
	assert_false(bt_uuid_cmp(&u.uuid, BT_UUID_DECLARE_16(0x0001)) == 0);

	/* Compare UUID 128 bits */
	assert_false(bt_uuid_cmp(&u.uuid, &le_128.uuid) == 0);

	/* Compare swapped UUID 16 bits */
	assert_true(bt_uuid_cmp(&u.uuid, BT_UUID_DECLARE_16(0x0100)) == 0);
}





static const struct CMUnitTest test_cases_bt_uuid_create[] = {
	cmocka_unit_test(bt_uuid_create__test_uuid_create),
};
BT_TEST_SUITE(bt_uuid_create, test_cases_bt_uuid_create, bt_test_host_setup, NULL);
