#include "support/test_support.h"
/*
 * SPDX-FileCopyrightText: Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <embt/base/byteorder.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/assigned_numbers.h>
#include <embt/bluetooth/classic/classic.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci.h>
#include <embt/base/log.h>

LOG_MODULE_REGISTER(test_gap_discovery, LOG_LEVEL_DBG);

bt_addr_t peer_addr;
static bt_atomic_t peer_cod;
static uint8_t connected_err;
static os_sem_t discovery_sem;
static os_sem_t connected_sem;
static os_sem_t disconnected_sem;

static void br_discover_timeout(const struct bt_br_discovery_result *results, size_t count)
{
	LOG_DBG("BR discovery done, found %zu devices", count);
	osi_sem_give(&discovery_sem);
}

static void br_discover_recv(const struct bt_br_discovery_result *result)
{
	if (bt_addr_eq(&peer_addr, &result->addr)) {
		bt_atomic_set(&peer_cod, sys_get_le24(result->cod));
		osi_sem_give(&discovery_sem);
	}
}

static void br_connected(struct bt_conn *conn, uint8_t conn_err)
{
	LOG_DBG("connected: conn %p err 0x%02x", (void *)conn, conn_err);
	connected_err = conn_err;
	osi_sem_give(&connected_sem);
	if (conn_err == 0) {
		puts("PEER_CONNECTED");
		fflush(stdout);
	}
}

static void br_disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_DBG("disconnected: conn %p reason 0x%02x", (void *)conn, reason);
	osi_sem_give(&disconnected_sem);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = br_connected,
	.disconnected = br_disconnected,
};

static bool peer_device_discovery(bool limited, uint8_t length)
{
	static struct bt_br_discovery_result results[10];
	struct bt_br_discovery_param param = {
		.length = length,
		.limited = limited,
	};
	int err;

	bt_atomic_set(&peer_cod, -1);
	osi_sem_reset(&discovery_sem);
	memset(results, 0, sizeof(results));

	err = bt_br_discovery_start(&param, results, ARRAY_SIZE(results));
	assert_int_equal(err, 0);

	err = osi_sem_take(&discovery_sem, BT_MSEC(length * 1280U + 5000U));
	assert_int_equal(err, 0);

	err = bt_br_discovery_stop();
	assert_true(err == 0 || err == -EALREADY);

	return bt_atomic_get(&peer_cod) != -1;
}

static void peer_device_connect(void)
{
	struct bt_conn *conn;
	int err;

	conn = bt_conn_create_br(&peer_addr, BT_BR_CONN_PARAM_DEFAULT);
	assert_non_null(conn);

	err = osi_sem_take(&connected_sem, BT_SECONDS(30));
	assert_int_equal(err, 0);
	assert_int_equal(connected_err, 0);

	/* The pair runner lets both Hosts observe connection before teardown. */
	if (getenv("EMBT_TEST_PEER_SYNC")) {
		assert_int_not_equal(getchar(), EOF);
	}

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	assert_int_equal(err, 0);

	err = osi_sem_take(&disconnected_sem, BT_SECONDS(30));
	assert_int_equal(err, 0);
	bt_conn_unref(conn);
}

static void gap_central__test_01_general_discovery(void **cm_state)
{
	(void)cm_state;
	assert_true(peer_device_discovery(false, 10U));
	peer_device_connect();
}

static void gap_central__test_02_limited_discovery(void **cm_state)
{
	(void)cm_state;
	assert_true(peer_device_discovery(true, 10U));
	assert_int_not_equal(bt_atomic_get(&peer_cod) & BT_COD_MAJOR_SVC_CLASS_LIMITED_DISCOVER, 0);

	osi_sleep_ms(BT_SECONDS(CONFIG_BT_LIMITED_DISCOVERABLE_DURATION + 1));

	assert_false(peer_device_discovery(true, 2U));
	peer_device_connect();
}

static void wait_for_peer_connection(void)
{
	int err;

	err = osi_sem_take(&connected_sem, BT_SECONDS(60));
	assert_int_equal(err, 0);
	assert_int_equal(connected_err, 0);

	err = osi_sem_take(&disconnected_sem, BT_SECONDS(30));
	assert_int_equal(err, 0);
}

static void gap_peripheral__test_01_general_discovery(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_br_set_connectable(true, NULL);
	assert_int_equal(err, 0);

	err = bt_br_set_discoverable(true, false);
	assert_int_equal(err, 0);

	puts("DISCOVERABLE"); fflush(stdout);
	wait_for_peer_connection();

	err = bt_br_set_discoverable(false, false);
	assert_int_equal(err, 0);

	err = bt_br_set_connectable(false, NULL);
	assert_int_equal(err, 0);
}

static void gap_peripheral__test_02_limited_discovery(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_br_set_connectable(true, NULL);
	assert_int_equal(err, 0);

	err = bt_br_set_discoverable(true, true);
	assert_int_equal(err, 0);

	puts("DISCOVERABLE"); fflush(stdout);
	wait_for_peer_connection();

	err = bt_br_set_connectable(false, NULL);
	assert_int_equal(err, 0);
}

static void *setup(void)
{
	static struct bt_br_discovery_cb br_discover = {
		.recv = br_discover_recv,
		.timeout = br_discover_timeout,
	};
	int err;

	assert_int_equal(bt_conn_cb_register(&bt_conn_cb_conn_callbacks), 0);
	err = bt_enable(NULL);
	assert_int_equal(err, 0);
	bt_br_discovery_cb_register(&br_discover);

	return NULL;
}

static void teardown(void *fixture)
{
	int err;

	err = bt_disable();
	assert_int_equal(err, 0);
}

static void before(void *fixture)
{
	osi_sem_reset(&discovery_sem);
	osi_sem_reset(&connected_sem);
	osi_sem_reset(&disconnected_sem);
}





static void storage_init_f1dbe2bcb6(void)
{
	bt_test_require(osi_sem_init(&discovery_sem, 0, 1));
	bt_test_require(osi_sem_init(&connected_sem, 0, 1));
	bt_test_require(osi_sem_init(&disconnected_sem, 0, 1));
}

int bt_test_gap_peripheral_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	storage_init_f1dbe2bcb6();
	*state = setup();
	return 0;
}
static int bt_test_gap_peripheral_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}

int bt_test_gap_peripheral_teardown(void **state)
{
	(void)state;
	teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_gap_peripheral[] = {
	cmocka_unit_test_setup_teardown(gap_peripheral__test_01_general_discovery, bt_test_gap_peripheral_before, NULL),
	cmocka_unit_test_setup_teardown(gap_peripheral__test_02_limited_discovery, bt_test_gap_peripheral_before, NULL),
};
BT_TEST_SUITE(gap_peripheral, test_cases_gap_peripheral, bt_test_gap_peripheral_setup, bt_test_gap_peripheral_teardown);

int bt_test_gap_central_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	storage_init_f1dbe2bcb6();
	*state = setup();
	return 0;
}
static int bt_test_gap_central_before(void **state)
{
	(void)state;
	before(*state);
	return 0;
}

int bt_test_gap_central_teardown(void **state)
{
	(void)state;
	teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_gap_central[] = {
	cmocka_unit_test_setup_teardown(gap_central__test_01_general_discovery, bt_test_gap_central_before, NULL),
	cmocka_unit_test_setup_teardown(gap_central__test_02_limited_discovery, bt_test_gap_central_before, NULL),
};
BT_TEST_SUITE(gap_central, test_cases_gap_central, bt_test_gap_central_setup, bt_test_gap_central_teardown);
