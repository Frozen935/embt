#include "support/test_support.h"
#include <embt/base/log.h>
/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/hci.h>
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define TIMEOUT_MS 300000 /* 5 minutes */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
};

/* Set Scan Response data */
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};



static void adv_tests__test_adv_fast_ad_data_update(void **cm_state)
{
	(void)cm_state;
	int err;

	bt_log_print("Starting Beacon Demo\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	assert_int_equal(err, 0);

	bt_log_print("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad),
			      sd, ARRAY_SIZE(sd));
	assert_int_equal(err, 0);

	bt_log_print("Advertising started\n");

	const int64_t started = bt_uptime_get_ms();
	while (bt_uptime_get_ms() - started < TIMEOUT_MS) {
		err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad),
					    sd, ARRAY_SIZE(sd));
		assert_int_equal(err, 0);
	}
	assert_int_equal(bt_le_adv_stop(), 0);
	assert_int_equal(bt_disable(), 0);
}





static const struct CMUnitTest test_cases_adv_tests[] = {
	cmocka_unit_test(adv_tests__test_adv_fast_ad_data_update),
};
BT_TEST_SUITE(adv_tests, test_cases_adv_tests, bt_test_base_setup, NULL);
