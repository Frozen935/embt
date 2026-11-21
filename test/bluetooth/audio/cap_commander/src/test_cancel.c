#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_d38d7b(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_d38d7b(const struct bt_test_case_info *, void *);


/* test_cancel.c - unit test for cancel command */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/hci_types.h>
#include <fff.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
#include "cap_commander.h"
#include "conn.h"
#include "expects_util.h"
#include "cap_mocks.h"
#include "test_common.h"
#include "bap_broadcast_assistant.h"

#define FFF_GLOBALS

struct cap_commander_test_cancel_fixture {
	struct bt_conn conns[CONFIG_BT_MAX_CONN];

	struct bt_bap_bass_subgroup subgroups[CONFIG_BT_BAP_BASS_MAX_SUBGROUPS];
	struct bt_cap_commander_broadcast_reception_start_member_param
		start_member_params[CONFIG_BT_MAX_CONN];
	struct bt_cap_commander_broadcast_reception_start_param start_param;
};

static void test_start_param_init(void *f)
{
	struct cap_commander_test_cancel_fixture *fixture = f;
	int err;

	fixture->start_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->start_param.param = fixture->start_member_params;

	fixture->start_param.count = ARRAY_SIZE(fixture->start_member_params);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->subgroups); i++) {
		fixture->subgroups[i].bis_sync = BIT(i);
		fixture->subgroups[i].metadata_len = 0;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->start_member_params); i++) {
		fixture->start_member_params[i].member.member = &fixture->conns[i];
		bt_addr_le_copy(&fixture->start_member_params[i].addr, BT_ADDR_LE_ANY);
		fixture->start_member_params[i].adv_sid = 0;
		fixture->start_member_params[i].pa_interval = 10;
		fixture->start_member_params[i].broadcast_id = 0;
		memcpy(fixture->start_member_params[i].subgroups, &fixture->subgroups[0],
		       sizeof(struct bt_bap_bass_subgroup) * CONFIG_BT_BAP_BASS_MAX_SUBGROUPS);
		fixture->start_member_params[i].num_subgroups = CONFIG_BT_BAP_BASS_MAX_SUBGROUPS;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		err = bt_cap_commander_discover(&fixture->conns[i]);
		assert_int_equal(0, err);
	}
}

static void
cap_commander_test_cancel_fixture_init(struct cap_commander_test_cancel_fixture *fixture)
{
	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		test_conn_init(&fixture->conns[i]);
	}

	test_start_param_init(fixture);
}

static void *cap_commander_test_cancel_setup(void)
{
	struct cap_commander_test_cancel_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_commander_test_cancel_before(void *f)
{
	struct cap_commander_test_cancel_fixture *fixture = f;

	memset(f, 0, sizeof(struct cap_commander_test_cancel_fixture));
	cap_commander_test_cancel_fixture_init(fixture);
}

static void cap_commander_test_cancel_after(void *f)
{
	struct cap_commander_test_cancel_fixture *fixture = f;
	int err;

	err = bt_cap_commander_unregister_cb(&mock_cap_commander_cb);
	assert_true(err == 0 || err == -EINVAL);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		mock_bt_conn_disconnected(&fixture->conns[i], BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void cap_commander_test_cancel_teardown(void *f)
{
	free(f);
}

static void test_cancel(void)
{
	int err;

	err = bt_cap_commander_cancel();
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 1,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
	assert_int_equal(-ECANCELED, mock_cap_commander_broadcast_reception_start_cb_fake.arg1_history[0]);
}



static void cap_commander_test_cancel__test_commander_cancel(void **cm_state)
{
	struct cap_commander_test_cancel_fixture *fixture = *cm_state;
	int err;

	if (CONFIG_BT_MAX_CONN == 1) {
		skip();
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	/* Do not run the add_src callback, so that the broadcast reception start procedure does not
	 * run until completion
	 */
	set_skip_add_src(1);

	/* initiate a CAP procedure; for this test we use broadcast reception start*/
	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(0, err);

	test_cancel();
}

static void cap_commander_test_cancel__test_commander_cancel_double(void **cm_state)
{
	struct cap_commander_test_cancel_fixture *fixture = *cm_state;
	int err;

	if (CONFIG_BT_MAX_CONN == 1) {
		skip();
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	set_skip_add_src(1);
	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(0, err);

	test_cancel();

	err = bt_cap_commander_cancel();
	assert_int_equal(-EALREADY, err);
}

static void cap_commander_test_cancel__test_commander_cancel_no_proc_in_progress(void **cm_state)
{
	struct cap_commander_test_cancel_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	err = bt_cap_commander_cancel();
	assert_int_equal(-EALREADY, err);
}

int bt_test_cap_commander_test_cancel_setup(void **state)
{
	int environment_result = bt_test_audio_cap_commander_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_commander_test_cancel_setup();
	return 0;
}
static int bt_test_cap_commander_test_cancel_before(void **state)
{
	(void)state;
	cap_commander_test_cancel_before(*state);
	mock_init_rule_before_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_cancel_after(void **state)
{
	(void)state;
	cap_commander_test_cancel_after(*state);
	mock_destroy_rule_after_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_cancel_teardown(void **state)
{
	(void)state;
	cap_commander_test_cancel_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_commander_test_cancel[] = {
	cmocka_unit_test_setup_teardown(cap_commander_test_cancel__test_commander_cancel, bt_test_cap_commander_test_cancel_before, bt_test_cap_commander_test_cancel_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_cancel__test_commander_cancel_double, bt_test_cap_commander_test_cancel_before, bt_test_cap_commander_test_cancel_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_cancel__test_commander_cancel_no_proc_in_progress, bt_test_cap_commander_test_cancel_before, bt_test_cap_commander_test_cancel_after),
};
BT_TEST_SUITE(cap_commander_test_cancel, test_cases_cap_commander_test_cancel, bt_test_cap_commander_test_cancel_setup, bt_test_cap_commander_test_cancel_teardown);
