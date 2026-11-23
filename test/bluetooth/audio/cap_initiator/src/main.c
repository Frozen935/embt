#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_05ae85(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_05ae85(const struct bt_test_case_info *, void *);

void mock_init_rule_before_05ae85(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_05ae85(const struct bt_test_case_info *, void *);

/* main.c - Application main entry point */

/*
 * Copyright (c) 2023-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/hci_types.h>
#include <fff.h>
#include <embt/base/util.h>
#include <embt/compiler.h>
#include "cap_initiator.h"
#include "conn.h"
#include "expects_util.h"
#include "test_common.h"

void mock_init_rule_before_05ae85(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	test_mocks_init();
}

void mock_destroy_rule_after_05ae85(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	test_mocks_cleanup();
}



struct cap_initiator_test_suite_fixture {
	struct bt_conn conns[CONFIG_BT_MAX_CONN];
};

static void cap_initiator_test_suite_fixture_init(struct cap_initiator_test_suite_fixture *fixture)
{
	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		test_conn_init(&fixture->conns[i], i);
	}
}

static void *cap_initiator_test_suite_setup(void)
{
	struct cap_initiator_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_initiator_test_suite_before(void *f)
{
	memset(f, 0, sizeof(struct cap_initiator_test_suite_fixture));
	cap_initiator_test_suite_fixture_init(f);
}

static void cap_initiator_test_suite_after(void *f)
{
	struct cap_initiator_test_suite_fixture *fixture = f;
	int err;

	err = bt_cap_initiator_unregister_cb(&mock_cap_initiator_cb);
	assert_true(err == 0 || err == -EINVAL);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		mock_bt_conn_disconnected(&fixture->conns[i], BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void cap_initiator_test_suite_teardown(void *f)
{
	free(f);
}



static void cap_initiator_test_suite__test_initiator_register_cb(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);
}

static void cap_initiator_test_suite__test_initiator_register_cb_inval_param_null(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_initiator_test_suite__test_initiator_register_cb_inval_double_register(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(-EALREADY, err);
}

static void cap_initiator_test_suite__test_initiator_unregister_cb(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	err = bt_cap_initiator_unregister_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);
}

static void cap_initiator_test_suite__test_initiator_unregister_cb_inval_param_null(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_unregister_cb(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_initiator_test_suite__test_initiator_unregister_cb_inval_double_unregister(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	err = bt_cap_initiator_unregister_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	err = bt_cap_initiator_unregister_cb(&mock_cap_initiator_cb);
	assert_int_equal(-EINVAL, err);
}

static void cap_initiator_test_suite__test_initiator_discover(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		err = bt_cap_initiator_unicast_discover(&fixture->conns[i]);
		assert_int_equal(0, err);
	}

	zexpect_call_count("bt_cap_initiator_cb.discovery_complete", ARRAY_SIZE(fixture->conns),
			   mock_cap_initiator_unicast_discovery_complete_cb_fake.call_count);
}

static void cap_initiator_test_suite__test_initiator_discover_inval_param_null(void **cm_state)
{
	struct cap_initiator_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	err = bt_cap_initiator_unicast_discover(NULL);
	assert_int_equal(-EINVAL, err);
}

int bt_test_cap_initiator_test_suite_setup(void **state)
{
	int environment_result = bt_test_audio_cap_initiator_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_initiator_test_suite_setup();
	return 0;
}
static int bt_test_cap_initiator_test_suite_before(void **state)
{
	(void)state;
	cap_initiator_test_suite_before(*state);
	mock_init_rule_before_05ae85(NULL, *state);
	return 0;
}
int bt_test_cap_initiator_test_suite_after(void **state)
{
	(void)state;
	cap_initiator_test_suite_after(*state);
	mock_destroy_rule_after_05ae85(NULL, *state);
	return 0;
}
int bt_test_cap_initiator_test_suite_teardown(void **state)
{
	(void)state;
	cap_initiator_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_initiator_test_suite[] = {
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_register_cb, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_register_cb_inval_param_null, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_register_cb_inval_double_register, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_unregister_cb, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_unregister_cb_inval_param_null, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_unregister_cb_inval_double_unregister, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_discover, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_suite__test_initiator_discover_inval_param_null, bt_test_cap_initiator_test_suite_before, bt_test_cap_initiator_test_suite_after),
};
BT_TEST_SUITE(cap_initiator_test_suite, test_cases_cap_initiator_test_suite, bt_test_cap_initiator_test_suite_setup, bt_test_cap_initiator_test_suite_teardown);
