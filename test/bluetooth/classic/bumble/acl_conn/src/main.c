#include "support/test_support.h"
/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/classic/classic.h>
#include <embt/base/log.h>

LOG_MODULE_REGISTER(test_acl_conn, LOG_LEVEL_DBG);

bt_addr_t peer_addr;

/* One semaphore per event: the peer may disconnect before the test thread has
 * seen the connection, and the state alone would then no longer show it.
 */
static os_sem_t connected_sem;
static os_sem_t disconnected_sem;
static struct bt_conn *acl_conn;
static uint8_t connected_err;

static void br_connected(struct bt_conn *conn, uint8_t conn_err)
{
	LOG_DBG("connected: conn %p err 0x%02x", (void *)conn, conn_err);
	if (conn_err == 0 && acl_conn == NULL) {
		acl_conn = bt_conn_ref(conn);
	}
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
	if (acl_conn != NULL) {
		bt_conn_unref(acl_conn);
		acl_conn = NULL;
	}
	osi_sem_give(&disconnected_sem);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = br_connected,
	.disconnected = br_disconnected,
};

static void *bt_setup(void)
{
	int err;

	err = bt_enable(NULL);
	assert_int_equal(err, 0);

	return NULL;
}

/* The central pages the peer directly by its (known) address, verifies the
 * connection comes up, then disconnects and verifies it comes down.
 */
static void acl_central__test_01_connect(void **cm_state)
{
	(void)cm_state;
	struct bt_conn *conn;
	int err;

	conn = bt_conn_create_br(&peer_addr, BT_BR_CONN_PARAM_DEFAULT);
	assert_true(conn != NULL);

	err = osi_sem_take(&connected_sem, BT_SECONDS(CONFIG_BT_CREATE_CONN_TIMEOUT));
	assert_int_equal(err, 0);
	assert_int_equal(connected_err, 0);
	assert_int_equal(acl_conn, conn);
	bt_conn_unref(conn);

	/* Wait until the pair runner observes the peer Host connected callback. */
	if (getenv("EMBT_TEST_PEER_SYNC")) {
		assert_int_not_equal(getchar(), EOF);
	}

	err = bt_conn_disconnect(acl_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	assert_int_equal(err, 0);

	err = osi_sem_take(&disconnected_sem, BT_SECONDS(30));
	assert_int_equal(err, 0);
	assert_true(acl_conn == NULL);
}



/* The peripheral makes itself connectable and waits for the central to
 * connect and then disconnect.
 */
static void acl_peripheral__test_01_connect(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_br_set_connectable(true, NULL);
	assert_int_equal(err, 0);
	puts("CONNECTABLE");
	fflush(stdout);

	err = osi_sem_take(&connected_sem, BT_SECONDS(60));
	assert_int_equal(err, 0);
	assert_int_equal(connected_err, 0);

	err = osi_sem_take(&disconnected_sem, BT_SECONDS(30));
	assert_int_equal(err, 0);
	assert_true(acl_conn == NULL);

	err = bt_br_set_connectable(false, NULL);
	assert_int_equal(err, 0);
}




static void storage_init_6326eb5978(void)
{
	bt_test_require(osi_sem_init(&connected_sem, 0, 1));
	bt_test_require(osi_sem_init(&disconnected_sem, 0, 1));
}

static void migrated_callbacks_init(void)
{
 static bool initialized;
 if (initialized) { return; }
 bt_test_require(bt_conn_cb_register(&bt_conn_cb_conn_callbacks));
 initialized = true;
}

static int bt_test_acl_peripheral_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	storage_init_6326eb5978();
	*state = bt_setup();
	return 0;
}



static const struct CMUnitTest test_cases_acl_peripheral[] = {
	cmocka_unit_test(acl_peripheral__test_01_connect),
};
BT_TEST_SUITE(acl_peripheral, test_cases_acl_peripheral, bt_test_acl_peripheral_setup, NULL);

static int bt_test_acl_central_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_callbacks_init();
	storage_init_6326eb5978();
	*state = bt_setup();
	return 0;
}



static const struct CMUnitTest test_cases_acl_central[] = {
	cmocka_unit_test(acl_central__test_01_connect),
};
BT_TEST_SUITE(acl_central, test_cases_acl_central, bt_test_acl_central_setup, NULL);
