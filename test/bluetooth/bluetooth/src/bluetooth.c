#include "support/test_support.h"
static const struct bt_hci_transport test_driver;
#include "host/hci_core.h"
/* bluetooth.c - Bluetooth smoke test */

/*
 * Copyright (c) 2015-2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */



#include <errno.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/drivers/bluetooth.h>


#define EXPECTED_ERROR -ENOSYS

static int driver_open(const struct bt_hci_transport *dev)
{
	printf("driver: %s\n", __func__);

	/* Indicate that there is no real Bluetooth device */
	return EXPECTED_ERROR;
}

static int driver_send(const struct bt_hci_transport *dev, struct net_buf *buf)
{
	return 0;
}

static const struct bt_hci_driver_api driver_api = {
	.open = driver_open,
	.send = driver_send,
};

static struct bt_hci_driver_data test_driver_data;
static const struct bt_hci_transport test_driver = {
 .name = "upstream-test", .bus = 0,
 .api = &driver_api, .data = &test_driver_data,
};
static void test_transport_init(void)
{
 bt_test_require(bt_stack_init());
 bt_dev.hci = &test_driver;
}




static void test_bluetooth__test_bluetooth_entry(void **cm_state)
{
	(void)cm_state;
	assert_true((bt_enable(NULL) == EXPECTED_ERROR));
}

int bt_test_test_bluetooth_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	test_transport_init();
	*state = NULL;
	return 0;
}



static const struct CMUnitTest test_cases_test_bluetooth[] = {
	cmocka_unit_test(test_bluetooth__test_bluetooth_entry),
};
BT_TEST_SUITE(test_bluetooth, test_cases_test_bluetooth, bt_test_test_bluetooth_setup, NULL);
