#include "support/test_support.h"
#include "test_environment.h"

/* test_procedures.c - Testing of CCP procedures  */

/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/assigned_numbers.h>
#include <embt/bluetooth/att.h>
#include <embt/bluetooth/audio/ccp.h>
#include <embt/bluetooth/audio/tbs.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
#include <embt/base/utf8.h>
#include "conn.h"
#include "test_common.h"

struct ccp_call_control_client_procedures_test_suite_fixture {
	struct bt_ccp_call_control_client_cb client_cbs;
	struct bt_ccp_call_control_client *client;
	struct bt_conn conn;

	/* Callback values */
	struct bt_ccp_call_control_client_bearer
		*bearers[CONFIG_BT_CCP_CALL_CONTROL_CLIENT_BEARER_COUNT];
	char bearer_name[CONFIG_BT_TBS_MAX_PROVIDER_NAME_LENGTH];
	char bearer_uci[BT_TBS_MAX_UCI_SIZE];
	char bearer_uri_schemes[BT_ATT_MAX_ATTRIBUTE_LEN + 1 /* NULL terminator*/];
	enum bt_bearer_tech tech;
};

static void discover_cb(struct bt_ccp_call_control_client *client, int err,
			struct bt_ccp_call_control_client_bearers *bearers, void *user_data)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = user_data;

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

static void bearer_provider_name_cb(struct bt_ccp_call_control_client_bearer *bearer, int err,
				    const char *name, void *user_data)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = user_data;

	assert_non_null(bearer);
	assert_int_equal(err, 0);
	assert_non_null(name);
	assert_non_null(user_data);
	assert_true(strlen(name) < CONFIG_BT_TBS_MAX_PROVIDER_NAME_LENGTH);
	assert_true(strlen(fixture->bearer_name) == 0U); /* expect only a single call */
	utf8_lcpy(fixture->bearer_name, name, CONFIG_BT_TBS_MAX_PROVIDER_NAME_LENGTH);
}

static void bearer_uci_cb(struct bt_ccp_call_control_client_bearer *bearer, int err,
			  const char *uci, void *user_data)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = user_data;

	assert_non_null(bearer);
	assert_int_equal(err, 0);

	assert_non_null(uci);
	assert_non_null(user_data);
	assert_true(strlen(uci) < BT_TBS_MAX_UCI_SIZE);
	assert_true(strlen(fixture->bearer_uci) == 0U); /* expect only a single call */

	utf8_lcpy(fixture->bearer_uci, uci, BT_TBS_MAX_UCI_SIZE);
}

static void bearer_tech_cb(struct bt_ccp_call_control_client_bearer *bearer, int err,
			   enum bt_bearer_tech tech, void *user_data)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = user_data;

	assert_non_null(bearer);
	assert_int_equal(err, 0);

	assert_non_null(user_data);

	fixture->tech = tech;
}

static void bearer_uri_schemes_cb(struct bt_ccp_call_control_client_bearer *bearer, int err,
				  const char *uri_schemes, void *user_data)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = user_data;

	assert_non_null(bearer);
	assert_int_equal(err, 0);

	assert_non_null(user_data);
	assert_non_null(uri_schemes);

	assert_true(strlen(uri_schemes) < sizeof(fixture->bearer_uri_schemes));
	assert_true(strlen(fixture->bearer_uri_schemes) == 0U); /* expect only a single call */
	utf8_lcpy(fixture->bearer_uri_schemes, uri_schemes, sizeof(fixture->bearer_uri_schemes));
}

static void *ccp_call_control_client_procedures_test_suite_setup(void)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void ccp_call_control_client_procedures_test_suite_before(void *f)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = f;
	int err;

	memset(fixture, 0, sizeof(*fixture));
	test_conn_init(&fixture->conn);

	fixture->client_cbs.discover = discover_cb;
	fixture->client_cbs.bearer_provider_name = bearer_provider_name_cb;
	fixture->client_cbs.bearer_uci = bearer_uci_cb;
	fixture->client_cbs.bearer_tech = bearer_tech_cb;
	fixture->client_cbs.bearer_uri_schemes = bearer_uri_schemes_cb;
	fixture->client_cbs.user_data = fixture;

	err = bt_ccp_call_control_client_register_cb(&fixture->client_cbs);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_client_discover(&fixture->conn, &fixture->client);
	assert_int_equal(err, 0);

	assert_non_null(fixture->bearers[0]);
}

static void ccp_call_control_client_procedures_test_suite_after(void *f)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = f;

	(void)bt_ccp_call_control_client_unregister_cb(&fixture->client_cbs);
	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void ccp_call_control_client_procedures_test_suite_teardown(void *f)
{
	free(f);
}



static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_provider_name(fixture->bearers[0]);
	assert_int_equal(err, 0);

	assert_true(strlen(fixture->bearer_name) > 0U);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_provider_name(NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name_inval_not_discovered(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	/* Fake disconnection to clear the discovered value for the bearers */
	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	/* Mark as connected again but without discovering */
	test_conn_init(&fixture->conn);

	err = bt_ccp_call_control_client_read_bearer_provider_name(fixture->bearers[0]);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name_inval_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	struct bt_ccp_call_control_client_bearer *invalid_bearer =
		(struct bt_ccp_call_control_client_bearer *)0xdeadbeefU;
	int err;

	err = bt_ccp_call_control_client_read_bearer_provider_name(invalid_bearer);
	assert_int_equal(err, -EEXIST);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_uci(fixture->bearers[0]);
	assert_int_equal(err, 0);

	assert_true(strlen(fixture->bearer_uci) > 0U);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_uci(NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci_inval_not_discovered(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	/* Fake disconnection to clear the discovered value for the bearers */
	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	/* Mark as connected again but without discovering */
	test_conn_init(&fixture->conn);

	err = bt_ccp_call_control_client_read_bearer_uci(fixture->bearers[0]);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci_inval_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	struct bt_ccp_call_control_client_bearer *invalid_bearer =
		(struct bt_ccp_call_control_client_bearer *)0xdeadbeefU;
	int err;

	err = bt_ccp_call_control_client_read_bearer_uci(invalid_bearer);
	assert_int_equal(err, -EEXIST);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_tech(fixture->bearers[0]);
	assert_int_equal(err, 0);

	assert_true(fixture->tech != 0);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_tech(NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech_inval_not_discovered(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	/* Fake disconnection to clear the discovered value for the bearers */
	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	/* Mark as connected again but without discovering */
	test_conn_init(&fixture->conn);

	err = bt_ccp_call_control_client_read_bearer_tech(fixture->bearers[0]);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech_inval_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	struct bt_ccp_call_control_client_bearer *invalid_bearer =
		(struct bt_ccp_call_control_client_bearer *)0xdeadbeefU;
	int err;

	err = bt_ccp_call_control_client_read_bearer_tech(invalid_bearer);
	assert_int_equal(err, -EEXIST);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_uri_schemes(fixture->bearers[0]);
	assert_int_equal(err, 0);

	assert_true(strlen(fixture->bearer_uri_schemes) > 0U);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_client_read_bearer_uri_schemes(NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes_inval_not_discovered(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	int err;

	/* Fake disconnection to clear the discovered value for the bearers*/
	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	/* Mark as connected again but without discovering */
	test_conn_init(&fixture->conn);

	err = bt_ccp_call_control_client_read_bearer_uri_schemes(fixture->bearers[0]);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes_inval_bearer(void **cm_state)
{
	struct ccp_call_control_client_procedures_test_suite_fixture *fixture = *cm_state;
	struct bt_ccp_call_control_client_bearer *invalid_bearer =
		(struct bt_ccp_call_control_client_bearer *)0xdeadbeefU;
	int err;

	err = bt_ccp_call_control_client_read_bearer_uri_schemes(invalid_bearer);
	assert_int_equal(err, -EEXIST);
}

int bt_test_ccp_call_control_client_procedures_test_suite_setup(void **state)
{
	int environment_result = bt_test_audio_ccp_call_control_client_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = ccp_call_control_client_procedures_test_suite_setup();
	return 0;
}
static int bt_test_ccp_call_control_client_procedures_test_suite_before(void **state)
{
	(void)state;
	ccp_call_control_client_procedures_test_suite_before(*state);
	return 0;
}
int bt_test_ccp_call_control_client_procedures_test_suite_after(void **state)
{
	(void)state;
	ccp_call_control_client_procedures_test_suite_after(*state);
	return 0;
}
int bt_test_ccp_call_control_client_procedures_test_suite_teardown(void **state)
{
	(void)state;
	ccp_call_control_client_procedures_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_ccp_call_control_client_procedures_test_suite[] = {
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name_inval_null_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name_inval_not_discovered, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_provider_name_inval_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci_inval_null_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci_inval_not_discovered, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uci_inval_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech_inval_null_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech_inval_not_discovered, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_tech_inval_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes_inval_null_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes_inval_not_discovered, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_client_procedures_test_suite__test_ccp_call_control_client_read_bearer_uri_schemes_inval_bearer, bt_test_ccp_call_control_client_procedures_test_suite_before, bt_test_ccp_call_control_client_procedures_test_suite_after),
};
BT_TEST_SUITE(ccp_call_control_client_procedures_test_suite, test_cases_ccp_call_control_client_procedures_test_suite, bt_test_ccp_call_control_client_procedures_test_suite_setup, bt_test_ccp_call_control_client_procedures_test_suite_teardown);
