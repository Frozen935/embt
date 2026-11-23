#include "support/test_support.h"
#include "test_environment.h"

/* main.c - Application main entry point */

/*
 * Copyright (c) 2024-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/audio/ccp.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/base/util_macro.h>
#include "conn.h"
#include "expects_util.h"
#include "test_common.h"

struct ccp_call_control_client_test_suite_fixture {
	struct bt_ccp_call_control_client_cb client_cbs;
	struct bt_ccp_call_control_client *client;
	struct bt_conn conn;

	/* Callback values */
	/** Need 1 additional bearer than the max to trigger some corner cases */
	struct bt_ccp_call_control_client_bearer
		*bearers[CONFIG_BT_CCP_CALL_CONTROL_CLIENT_BEARER_COUNT + 1];
};

static void discover_cb(struct bt_ccp_call_control_client *client, int err,
			struct bt_ccp_call_control_client_bearers *bearers, void *user_data)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = user_data;

	assert_non_null(client);
	assert_int_equal(err, 0);
	assert_non_null(bearers);
	assert_non_null(user_data);
	assert_null(fixture->bearers[0]); /* expect only a single call */

#if defined(CONFIG_BT_TBS_CLIENT_GTBS)
	assert_non_null(bearers->gtbs_bearer);
	fixture->bearers[0] = bearers->gtbs_bearer;
#endif /* CONFIG_BT_TBS_CLIENT_GTBS */

#if defined(CONFIG_BT_TBS_CLIENT_TBS)
	assert_int_equal(CONFIG_BT_TBS_CLIENT_MAX_TBS_INSTANCES, bearers->tbs_count);
	for (size_t i = 0U; i < bearers->tbs_count; i++) {
		assert_non_null(bearers->tbs_bearers[i]);
		fixture->bearers[i + IS_ENABLED(CONFIG_BT_TBS_CLIENT_GTBS)] =
			bearers->tbs_bearers[i];
	}
#endif /* CONFIG_BT_TBS_CLIENT_TBS */
}

static void *ccp_call_control_client_test_suite_setup(void)
{
	struct ccp_call_control_client_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void ccp_call_control_client_test_suite_before(void *f)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = f;

	memset(fixture, 0, sizeof(*fixture));
	test_conn_init(&fixture->conn);

	fixture->client_cbs.discover = discover_cb;
	fixture->client_cbs.user_data = fixture;
}

static void ccp_call_control_client_test_suite_after(void *f)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = f;

	(void)bt_ccp_call_control_client_unregister_cb(&fixture->client_cbs);
	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void ccp_call_control_client_test_suite_teardown(void *f)
{
	free(f);
}



static void ccp_call_control_client_test_suite__test_ccp_call_control_client_register_cb(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_register_cb_inval_param_null(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(NULL);
	assert_int_equal(-EINVAL, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_register_cb_inval_double_register(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(-EEXIST, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_unregister_cb(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_unregister_cb(&fixture->client_cbs);
	assert_int_equal(0, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_unregister_cb_inval_param_null(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_unregister_cb(NULL);
	assert_int_equal(-EINVAL, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_unregister_cb_inval_double_unregister(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_unregister_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_unregister_cb(&fixture->client_cbs);
	assert_int_equal(-EALREADY, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_discover(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_discover(&fixture->conn, &fixture->client);
	assert_int_equal(0, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_discover_inval_param_null_conn(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_discover(NULL, &fixture->client);
	assert_int_equal(-EINVAL, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_discover_inval_param_null_client(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_discover(&fixture->conn, NULL);
	assert_int_equal(-EINVAL, err);
}

static void ccp_call_control_client_test_suite__test_ccp_call_control_client_get_bearers(void **cm_state)
{
	struct ccp_call_control_client_test_suite_fixture *fixture = *cm_state;
	struct bt_ccp_call_control_client_bearers bearers;
	int err;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_discover(&fixture->conn, &fixture->client);
	assert_int_equal(0, err);

	err = bt_ccp_call_control_client_get_bearers(fixture->client, &bearers);
	assert_int_equal(0, err);

#if defined(CONFIG_BT_TBS_CLIENT_GTBS)
	assert_non_null(bearers.gtbs_bearer);
#endif /* CONFIG_BT_TBS_CLIENT_GTBS */

#if defined(CONFIG_BT_TBS_CLIENT_TBS)
	assert_int_equal(CONFIG_BT_TBS_CLIENT_MAX_TBS_INSTANCES, bearers.tbs_count);
	for (size_t i = 0U; i < bearers.tbs_count; i++) {
		assert_non_null(bearers.tbs_bearers[i]);
	}
#endif /* CONFIG_BT_TBS_CLIENT_TBS */
}

int bt_test_ccp_call_control_client_test_suite_setup(void **state)
{
	int environment_result = bt_test_audio_ccp_call_control_client_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = ccp_call_control_client_test_suite_setup();
	return 0;
}
static int bt_test_ccp_call_control_client_test_suite_before(void **state)
{
	(void)state;
	ccp_call_control_client_test_suite_before(*state);
	return 0;
}
int bt_test_ccp_call_control_client_test_suite_after(void **state)
{
	(void)state;
	ccp_call_control_client_test_suite_after(*state);
	return 0;
}
int bt_test_ccp_call_control_client_test_suite_teardown(void **state)
{
	(void)state;
	ccp_call_control_client_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_ccp_call_control_client_test_suite[] = {
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_register_cb, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_register_cb_inval_param_null, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_register_cb_inval_double_register, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_unregister_cb, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_unregister_cb_inval_param_null, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_unregister_cb_inval_double_unregister, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_discover, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_discover_inval_param_null_conn, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_discover_inval_param_null_client, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_test_suite__test_ccp_call_control_client_get_bearers, bt_test_ccp_call_control_client_test_suite_before, bt_test_ccp_call_control_client_test_suite_after),
};
BT_TEST_SUITE(ccp_call_control_client_test_suite, test_cases_ccp_call_control_client_test_suite, bt_test_ccp_call_control_client_test_suite_setup, bt_test_ccp_call_control_client_test_suite_teardown);
