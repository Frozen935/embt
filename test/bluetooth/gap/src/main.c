#include "support/test_support.h"
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <embt/bluetooth/gap.h>
static void gap_test_suite__test_bt_gap_conversion_macros(void **cm_state)
{
	(void)cm_state;
	assert_int_equal(BT_GAP_ADV_INTERVAL_TO_US(0x0020U), 20000U);
	assert_int_equal(BT_GAP_ADV_INTERVAL_TO_US(0x0021U), 20625U);
	assert_int_equal(BT_GAP_ADV_INTERVAL_TO_US(0x0022U), 21250U);

	assert_int_equal(BT_GAP_ADV_INTERVAL_TO_MS(0x0020U), 20U);
	/* Round down expected from 20.625 */
	assert_int_equal(BT_GAP_ADV_INTERVAL_TO_MS(0x0021U), 20U);
	/* Round down expected from 21.250 */
	assert_int_equal(BT_GAP_ADV_INTERVAL_TO_MS(0x0022U), 21U);

	assert_int_equal(BT_GAP_ISO_INTERVAL_TO_US(0x0004U), 5000U);
	assert_int_equal(BT_GAP_ISO_INTERVAL_TO_US(0x0005U), 6250U);
	assert_int_equal(BT_GAP_ISO_INTERVAL_TO_US(0x0006U), 7500U);

	assert_int_equal(BT_GAP_ISO_INTERVAL_TO_MS(0x0004U), 5U);
	/* Round down expected from 6.25 */
	assert_int_equal(BT_GAP_ISO_INTERVAL_TO_MS(0x0005U), 6U);
	/* Round down expected from 7.50 */
	assert_int_equal(BT_GAP_ISO_INTERVAL_TO_MS(0x0006U), 7U);

	assert_int_equal(BT_GAP_PER_ADV_INTERVAL_TO_US(0x0008U), 10000U);
	assert_int_equal(BT_GAP_PER_ADV_INTERVAL_TO_US(0x0009U), 11250U);
	assert_int_equal(BT_GAP_PER_ADV_INTERVAL_TO_US(0x000aU), 12500U);

	assert_int_equal(BT_GAP_PER_ADV_INTERVAL_TO_MS(0x0008U), 10U);
	/* Round down expected from 11.25 */
	assert_int_equal(BT_GAP_PER_ADV_INTERVAL_TO_MS(0x0009U), 11U);
	/* Round down expected from 12.50 */
	assert_int_equal(BT_GAP_PER_ADV_INTERVAL_TO_MS(0x000aU), 12U);

	assert_int_equal(BT_GAP_US_TO_ADV_INTERVAL(20000U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_US_TO_ADV_INTERVAL(21000U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_US_TO_ADV_INTERVAL(22000U), 0x0023U);

	assert_int_equal(BT_GAP_MS_TO_ADV_INTERVAL(20U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_MS_TO_ADV_INTERVAL(21U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_MS_TO_ADV_INTERVAL(22U), 0x0023U);

	assert_int_equal(BT_GAP_US_TO_PER_ADV_INTERVAL(10000U), 0x0008U);
	/* Round down expected from 8.8 */
	assert_int_equal(BT_GAP_US_TO_PER_ADV_INTERVAL(11000U), 0x0008U);
	/* Round down expected from 9.6 */
	assert_int_equal(BT_GAP_US_TO_PER_ADV_INTERVAL(12000U), 0x0009U);

	assert_int_equal(BT_GAP_MS_TO_PER_ADV_INTERVAL(10U), 0x0008U);
	/* Round down expected from 8.8 */
	assert_int_equal(BT_GAP_MS_TO_PER_ADV_INTERVAL(11U), 0x0008U);
	/* Round down expected from 9.6 */
	assert_int_equal(BT_GAP_MS_TO_PER_ADV_INTERVAL(12U), 0x0009U);

	assert_int_equal(BT_GAP_MS_TO_PER_ADV_SYNC_TIMEOUT(4000U), 0x0190U);
	/* Round down expected from 400.5 */
	assert_int_equal(BT_GAP_MS_TO_PER_ADV_SYNC_TIMEOUT(4005U), 0x0190U);

	assert_int_equal(BT_GAP_US_TO_PER_ADV_SYNC_TIMEOUT(4000000U), 0x0190U);
	/* Round down expected from 400.5 */
	assert_int_equal(BT_GAP_US_TO_PER_ADV_SYNC_TIMEOUT(4005000U), 0x0190U);

	assert_int_equal(BT_GAP_US_TO_SCAN_INTERVAL(20000U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_US_TO_SCAN_INTERVAL(21000U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_US_TO_SCAN_INTERVAL(22000U), 0x0023U);

	assert_int_equal(BT_GAP_MS_TO_SCAN_INTERVAL(20U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_MS_TO_SCAN_INTERVAL(21U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_MS_TO_SCAN_INTERVAL(22U), 0x0023U);

	assert_int_equal(BT_GAP_US_TO_SCAN_WINDOW(20000U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_US_TO_SCAN_WINDOW(21000U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_US_TO_SCAN_WINDOW(22000U), 0x0023U);

	assert_int_equal(BT_GAP_MS_TO_SCAN_WINDOW(20U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_MS_TO_SCAN_WINDOW(21U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_MS_TO_SCAN_WINDOW(22U), 0x0023U);

	assert_int_equal(BT_GAP_US_TO_CONN_INTERVAL(10000U), 0x0008U);
	/* Round down expected from 8.8 */
	assert_int_equal(BT_GAP_US_TO_CONN_INTERVAL(11000U), 0x0008U);
	/* Round down expected from 9.6 */
	assert_int_equal(BT_GAP_US_TO_CONN_INTERVAL(12000U), 0x0009U);

	assert_int_equal(BT_GAP_MS_TO_CONN_INTERVAL(10U), 0x0008U);
	/* Round down expected from 8.8 */
	assert_int_equal(BT_GAP_MS_TO_CONN_INTERVAL(11U), 0x0008U);
	/* Round down expected from 9.6 */
	assert_int_equal(BT_GAP_MS_TO_CONN_INTERVAL(12U), 0x0009U);

	assert_int_equal(BT_GAP_MS_TO_CONN_TIMEOUT(4000U), 0x0190U);
	/* Round down expected from 400.5 */
	assert_int_equal(BT_GAP_MS_TO_CONN_TIMEOUT(4005U), 0x0190U);

	assert_int_equal(BT_GAP_US_TO_CONN_TIMEOUT(4000000U), 0x0190U);
	/* Round down expected from 400.5 */
	assert_int_equal(BT_GAP_US_TO_CONN_TIMEOUT(4005000U), 0x0190U);

	assert_int_equal(BT_GAP_US_TO_CONN_EVENT_LEN(20000U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_US_TO_CONN_EVENT_LEN(21000U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_US_TO_CONN_EVENT_LEN(22000U), 0x0023U);

	assert_int_equal(BT_GAP_MS_TO_CONN_EVENT_LEN(20U), 0x0020U);
	/* Round down expected from 33.60 */
	assert_int_equal(BT_GAP_MS_TO_CONN_EVENT_LEN(21U), 0x0021U);
	/* Round down expected from 35.20 */
	assert_int_equal(BT_GAP_MS_TO_CONN_EVENT_LEN(22U), 0x0023U);
}





static const struct CMUnitTest test_cases_gap_test_suite[] = {
	cmocka_unit_test(gap_test_suite__test_bt_gap_conversion_macros),
};
BT_TEST_SUITE(gap_test_suite, test_cases_gap_test_suite, bt_test_host_setup, NULL);
