#include "support/test_support.h"
void mock_init_rule_before_a2543a(const struct bt_test_case_info *, void *);
void mock_init_rule_before_17cc08(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_17cc08(const struct bt_test_case_info *, void *);

void mock_init_rule_before_a2543a(const struct bt_test_case_info *, void *);
void mock_init_rule_before_a2543a(const struct bt_test_case_info *, void *);

/* test_callback_register.c - Test bt_bap_broadcast_source_register and unregister */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>

#include <embt/bluetooth/audio/bap.h>
#include <fff.h>
#include <embt/compiler.h>
#include "bap_broadcast_source.h"

#define FFF_GLOBALS

void mock_init_rule_before_a2543a(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	mock_bap_broadcast_source_init();
}



static void bap_broadcast_source_test_cb_register_suite_after(void *f)
{
	int err;

	ARG_UNUSED(f);

	err = bt_bap_broadcast_source_unregister_cb(&mock_bap_broadcast_source_cb);
	assert_true(err == 0 || err == -ENOENT);
}



static void bap_broadcast_source_test_cb_register_suite__test_broadcast_source_register_cb(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_bap_broadcast_source_register_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(0, err);
}

static void bap_broadcast_source_test_cb_register_suite__test_broadcast_source_register_cb_inval_param_null(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_bap_broadcast_source_register_cb(NULL);
	assert_int_equal(err, -EINVAL);
}

static void bap_broadcast_source_test_cb_register_suite__test_broadcast_source_register_cb_inval_double_register(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_bap_broadcast_source_register_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_register_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, -EEXIST);
}

static void bap_broadcast_source_test_cb_register_suite__test_broadcast_source_unregister_cb(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_bap_broadcast_source_register_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_unregister_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, 0);
}

static void bap_broadcast_source_test_cb_register_suite__test_broadcast_source_unregister_cb_inval_param_null(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_bap_broadcast_source_unregister_cb(NULL);
	assert_int_equal(err, -EINVAL);
}

static void bap_broadcast_source_test_cb_register_suite__test_broadcast_source_unregister_cb_inval_double_unregister(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_bap_broadcast_source_register_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_unregister_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_unregister_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(err, -ENOENT);
}


static int bt_test_bap_broadcast_source_test_cb_register_suite_before(void **state)
{
	(void)state;
	mock_init_rule_before_a2543a(NULL, *state);
	mock_init_rule_before_17cc08(NULL, *state);
	return 0;
}
int bt_test_bap_broadcast_source_test_cb_register_suite_after(void **state)
{
	(void)state;
	bap_broadcast_source_test_cb_register_suite_after(*state);
	mock_destroy_rule_after_17cc08(NULL, *state);
	return 0;
}

static const struct CMUnitTest test_cases_bap_broadcast_source_test_cb_register_suite[] = {
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_cb_register_suite__test_broadcast_source_register_cb, bt_test_bap_broadcast_source_test_cb_register_suite_before, bt_test_bap_broadcast_source_test_cb_register_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_cb_register_suite__test_broadcast_source_register_cb_inval_param_null, bt_test_bap_broadcast_source_test_cb_register_suite_before, bt_test_bap_broadcast_source_test_cb_register_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_cb_register_suite__test_broadcast_source_register_cb_inval_double_register, bt_test_bap_broadcast_source_test_cb_register_suite_before, bt_test_bap_broadcast_source_test_cb_register_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_cb_register_suite__test_broadcast_source_unregister_cb, bt_test_bap_broadcast_source_test_cb_register_suite_before, bt_test_bap_broadcast_source_test_cb_register_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_cb_register_suite__test_broadcast_source_unregister_cb_inval_param_null, bt_test_bap_broadcast_source_test_cb_register_suite_before, bt_test_bap_broadcast_source_test_cb_register_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_cb_register_suite__test_broadcast_source_unregister_cb_inval_double_unregister, bt_test_bap_broadcast_source_test_cb_register_suite_before, bt_test_bap_broadcast_source_test_cb_register_suite_after),
};
BT_TEST_SUITE(bap_broadcast_source_test_cb_register_suite, test_cases_bap_broadcast_source_test_cb_register_suite, bt_test_base_setup, NULL);
