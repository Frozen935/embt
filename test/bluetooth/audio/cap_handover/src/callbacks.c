#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_744017(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_744017(const struct bt_test_case_info *, void *);
void mock_init_rule_before_127f12(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_127f12(const struct bt_test_case_info *, void *);


/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>

#include <embt/bluetooth/audio/cap.h>
#include <fff.h>
#include <embt/base/util.h>
#include <embt/compiler.h>
#include "cap_handover.h"
#include "test_common.h"

static void cap_handover_callbacks_test_suite_after(void *f)
{
	int err;

	ARG_UNUSED(f);

	err = bt_cap_handover_unregister_cb(&mock_cap_handover_cb);
	assert_true(err == 0 || err == -EINVAL);
}



static void cap_handover_callbacks_test_suite__test_handover_register_cb(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);
}

static void cap_handover_callbacks_test_suite__test_handover_register_cb_inval_param_null(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_cap_handover_register_cb(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_handover_callbacks_test_suite__test_handover_register_cb_inval_double_register(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(-EALREADY, err);
}

static void cap_handover_callbacks_test_suite__test_handover_unregister_cb(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);

	err = bt_cap_handover_unregister_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);
}

static void cap_handover_callbacks_test_suite__test_handover_unregister_cb_inval_param_null(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_cap_handover_unregister_cb(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_handover_callbacks_test_suite__test_handover_unregister_cb_inval_double_unregister(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);

	err = bt_cap_handover_unregister_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);

	err = bt_cap_handover_unregister_cb(&mock_cap_handover_cb);
	assert_int_equal(-EINVAL, err);
}


static int bt_test_cap_handover_callbacks_test_suite_before(void **state)
{
	(void)state;
	mock_init_rule_before_744017(NULL, *state);
	mock_init_rule_before_127f12(NULL, *state);
	return 0;
}
int bt_test_cap_handover_callbacks_test_suite_after(void **state)
{
	(void)state;
	cap_handover_callbacks_test_suite_after(*state);
	mock_destroy_rule_after_744017(NULL, *state);
	mock_destroy_rule_after_127f12(NULL, *state);
	return 0;
}

static const struct CMUnitTest test_cases_cap_handover_callbacks_test_suite[] = {
	cmocka_unit_test_setup_teardown(cap_handover_callbacks_test_suite__test_handover_register_cb, bt_test_cap_handover_callbacks_test_suite_before, bt_test_cap_handover_callbacks_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_callbacks_test_suite__test_handover_register_cb_inval_param_null, bt_test_cap_handover_callbacks_test_suite_before, bt_test_cap_handover_callbacks_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_callbacks_test_suite__test_handover_register_cb_inval_double_register, bt_test_cap_handover_callbacks_test_suite_before, bt_test_cap_handover_callbacks_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_callbacks_test_suite__test_handover_unregister_cb, bt_test_cap_handover_callbacks_test_suite_before, bt_test_cap_handover_callbacks_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_callbacks_test_suite__test_handover_unregister_cb_inval_param_null, bt_test_cap_handover_callbacks_test_suite_before, bt_test_cap_handover_callbacks_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_callbacks_test_suite__test_handover_unregister_cb_inval_double_unregister, bt_test_cap_handover_callbacks_test_suite_before, bt_test_cap_handover_callbacks_test_suite_after),
};
BT_TEST_SUITE(cap_handover_callbacks_test_suite, test_cases_cap_handover_callbacks_test_suite, bt_test_audio_cap_handover_environment_setup, NULL);
