#include "support/test_support.h"
#include "host/conn_internal.h"
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */



#include <errno.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/drivers/bluetooth.h>
#include <embt/base/byteorder.h>

static enum bt_buf_type freed_buf_type;
static os_sem_t rx_sem;

void bt_buf_rx_freed_cb(enum bt_buf_type type)
{
	freed_buf_type = type;
	osi_sem_give(&rx_sem);
}



static void test_buf_data_api__test_buf_freed_cb(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf;
	int err;

	bt_buf_rx_freed_cb_set(bt_buf_rx_freed_cb);

	/* Test that the callback is called for the BT_BUF_EVT type */
	buf = bt_buf_get_rx(BT_BUF_EVT, BT_NO_WAIT);
	assert_non_null(buf);

	net_buf_unref(buf);
	/* The freed buf cb is called from net_buf_unref, therefore the semaphore should
	 * already by given.
	 */
	err = osi_sem_take(&rx_sem, BT_NO_WAIT);
	assert_int_equal(err, 0);
	assert_int_equal(BT_BUF_EVT, BT_BUF_EVT & freed_buf_type);

	/* Test that the callback is called for the BT_BUF_ACL_IN type */
	buf = bt_buf_get_rx(BT_BUF_ACL_IN, BT_NO_WAIT);
	assert_non_null(buf);

	net_buf_unref(buf);
	/* The freed buf cb is called from net_buf_unref, therefore the semaphore should
	 * already by given.
	 */
	err = osi_sem_take(&rx_sem, BT_NO_WAIT);
	assert_int_equal(err, 0);
	assert_int_equal(BT_BUF_ACL_IN, BT_BUF_ACL_IN & freed_buf_type);

	/* Test that the callback is called for the BT_BUF_ISO_IN type */
	buf = bt_buf_get_rx(BT_BUF_ISO_IN, BT_NO_WAIT);
	assert_non_null(buf);

	net_buf_unref(buf);
	/* The freed buf cb is called from net_buf_unref, therefore the semaphore should
	 * already by given.
	 */
	err = osi_sem_take(&rx_sem, BT_NO_WAIT);
	assert_int_equal(err, 0);
	assert_int_equal(BT_BUF_ISO_IN, BT_BUF_ISO_IN & freed_buf_type);
}

static void storage_init_15407cb430(void)
{
	bt_test_require(osi_sem_init(&rx_sem, 0, 1));
}

int bt_test_test_buf_data_api_setup(void **state)
{
	int environment_result = bt_test_buffer_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	bt_test_select_unopened_hci();
#if !defined(CONFIG_BT_HCI_RAW)
	bt_test_require(bt_conn_init());
	bt_test_require(bt_conn_iso_init());
#endif
	storage_init_15407cb430();
	*state = NULL;
	return 0;
}



static const struct CMUnitTest test_cases_test_buf_data_api[] = {
	cmocka_unit_test(test_buf_data_api__test_buf_freed_cb),
};
BT_TEST_SUITE(test_buf_data_api, test_cases_test_buf_data_api, bt_test_test_buf_data_api_setup, NULL);
