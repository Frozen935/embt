#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_d38d7b(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_d38d7b(const struct bt_test_case_info *, void *);


/* test_distribute_broadcast_code.c - unit test for distribute broadcast code */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/hci_types.h>
#include <fff.h>
#include "cap_commander.h"
#include "conn.h"
#include "expects_util.h"
#include "cap_mocks.h"
#include "test_common.h"

#include <embt/base/log.h>
#include <embt/base/util.h>

LOG_MODULE_REGISTER(bt_distribute_broadcast_code, CONFIG_BT_CAP_COMMANDER_LOG_LEVEL);

#define FFF_GLOBALS

struct cap_commander_test_distribute_broadcast_code_fixture {
	struct bt_conn conns[CONFIG_BT_MAX_CONN];

	struct bt_bap_bass_subgroup subgroups[CONFIG_BT_BAP_BASS_MAX_SUBGROUPS];
	struct bt_cap_commander_broadcast_reception_start_member_param
		start_member_params[CONFIG_BT_MAX_CONN];
	struct bt_cap_commander_broadcast_reception_start_param start_param;
	struct bt_cap_commander_distribute_broadcast_code_member_param
		broadcast_code_member_params[CONFIG_BT_MAX_CONN];
	struct bt_cap_commander_distribute_broadcast_code_param distribute_broadcast_code_param;
	struct bt_bap_broadcast_assistant_cb broadcast_assistant_cb;
};

static void cap_commander_test_distribute_broadcast_code_fixture_init(
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture)
{
	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		test_conn_init(&fixture->conns[i]);
		fixture->conns[i].index = i;
	}

	fixture->distribute_broadcast_code_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->distribute_broadcast_code_param.param = fixture->broadcast_code_member_params;
	fixture->distribute_broadcast_code_param.count =
		ARRAY_SIZE(fixture->broadcast_code_member_params);
	memcpy(fixture->distribute_broadcast_code_param.broadcast_code, BROADCAST_CODE,
	       sizeof(BROADCAST_CODE));
	for (size_t i = 0U; i < ARRAY_SIZE(fixture->broadcast_code_member_params); i++) {
		fixture->broadcast_code_member_params[i].member.member = &fixture->conns[i];
		fixture->broadcast_code_member_params[i].src_id = RANDOM_SRC_ID;
	}
}

static void *cap_commander_test_distribute_broadcast_code_setup(void)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_commander_test_distribute_broadcast_code_before(void *f)
{
	int err;
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = f;

	memset(f, 0, sizeof(struct cap_commander_test_distribute_broadcast_code_fixture));
	cap_commander_test_distribute_broadcast_code_fixture_init(fixture);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		err = bt_cap_commander_discover(&fixture->conns[i]);
		assert_int_equal(0, err);
	}
}

static void cap_commander_test_distribute_broadcast_code_after(void *f)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = f;
	int err;

	err = bt_cap_commander_unregister_cb(&mock_cap_commander_cb);
	assert_true(err == 0 || err == -EINVAL);
	err = bt_bap_broadcast_assistant_unregister_cb(&fixture->broadcast_assistant_cb);
	assert_true(err == 0 || err == -EALREADY);

	/* We need to cleanup since the CAP commander remembers state */
	err = bt_cap_commander_cancel();
	assert_true(err == 0 || err == -EALREADY);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		mock_bt_conn_disconnected(&fixture->conns[i], BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void cap_commander_test_distribute_broadcast_code_teardown(void *f)
{
	free(f);
}

static void test_distribute_broadcast_code(
	struct bt_cap_commander_distribute_broadcast_code_param *distribute_broadcast_code_param)
{
	int err;

	err = bt_cap_commander_distribute_broadcast_code(distribute_broadcast_code_param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 1,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
	assert_ptr_equal(NULL, mock_cap_commander_distribute_broadcast_code_cb_fake.arg0_history[0]);
	assert_int_equal(0, mock_cap_commander_distribute_broadcast_code_cb_fake.arg1_history[0]);
}



static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_reception_distribute_broadcast_code_double(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);

	/*
	 * We can not use test_distribute_broadcast_code because of the check on how often the
	 * callback function is called
	 */
	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 2,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
	assert_ptr_equal(NULL, mock_cap_commander_distribute_broadcast_code_cb_fake.arg0_history[1]);
	assert_int_equal(0, mock_cap_commander_distribute_broadcast_code_cb_fake.arg1_history[1]);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_reception_distribute_broadcast_code_param_null(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_distribute_broadcast_code(NULL);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_param_zero_count(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	fixture->distribute_broadcast_code_param.count = 0;

	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_param_high_count(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	fixture->distribute_broadcast_code_param.count = CONFIG_BT_MAX_CONN + 1;

	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_inval_param_null_param(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	fixture->distribute_broadcast_code_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->distribute_broadcast_code_param.param = NULL;
	fixture->distribute_broadcast_code_param.count = ARRAY_SIZE(fixture->conns);

	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_inval_null_member(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	fixture->distribute_broadcast_code_param.param[0].member.member = NULL;

	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_inval_missing_cas(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	fixture->distribute_broadcast_code_param.type = BT_CAP_SET_TYPE_CSIP;

	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

static void cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_double_conn(void **cm_state)
{
	struct cap_commander_test_distribute_broadcast_code_fixture *fixture = *cm_state;
	int err;

	if (CONFIG_BT_MAX_CONN == 1) {
		skip();
	}

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->broadcast_code_member_params); i++) {
		fixture->broadcast_code_member_params[i].member.member = &fixture->conns[0];
	}

	err = bt_cap_commander_distribute_broadcast_code(&fixture->distribute_broadcast_code_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.distribute_broadcast_code", 0,
			   mock_cap_commander_distribute_broadcast_code_cb_fake.call_count);
}

int bt_test_cap_commander_test_distribute_broadcast_code_setup(void **state)
{
	int environment_result = bt_test_audio_cap_commander_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_commander_test_distribute_broadcast_code_setup();
	return 0;
}
static int bt_test_cap_commander_test_distribute_broadcast_code_before(void **state)
{
	(void)state;
	cap_commander_test_distribute_broadcast_code_before(*state);
	mock_init_rule_before_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_distribute_broadcast_code_after(void **state)
{
	(void)state;
	cap_commander_test_distribute_broadcast_code_after(*state);
	mock_destroy_rule_after_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_distribute_broadcast_code_teardown(void **state)
{
	(void)state;
	cap_commander_test_distribute_broadcast_code_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_commander_test_distribute_broadcast_code[] = {
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_reception_distribute_broadcast_code_double, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_reception_distribute_broadcast_code_param_null, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_param_zero_count, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_param_high_count, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_inval_param_null_param, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_inval_null_member, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_inval_missing_cas, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_distribute_broadcast_code__test_commander_distribute_broadcast_code_double_conn, bt_test_cap_commander_test_distribute_broadcast_code_before, bt_test_cap_commander_test_distribute_broadcast_code_after),
};
BT_TEST_SUITE(cap_commander_test_distribute_broadcast_code, test_cases_cap_commander_test_distribute_broadcast_code, bt_test_cap_commander_test_distribute_broadcast_code_setup, bt_test_cap_commander_test_distribute_broadcast_code_teardown);
