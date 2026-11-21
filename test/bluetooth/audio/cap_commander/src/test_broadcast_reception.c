#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_d38d7b(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_d38d7b(const struct bt_test_case_info *, void *);


/* test_broadcast_reception.c - unit test for broadcast reception start and stop */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/gap.h>
#include <embt/bluetooth/hci_types.h>
#include <fff.h>
#include <embt/base/log.h>
#include <embt/base/log.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
#include <embt/compiler.h>
#include "cap_commander.h"
#include "conn.h"
#include "expects_util.h"
#include "cap_mocks.h"
#include "test_common.h"

LOG_MODULE_REGISTER(bt_broadcast_reception_test, CONFIG_BT_CAP_COMMANDER_LOG_LEVEL);

#define FFF_GLOBALS

#define SID          0x0EU
#define ADV_INTERVAL 10U
#define BROADCAST_ID 0x55AA55U

struct cap_commander_test_broadcast_reception_fixture {
	struct bt_conn conns[CONFIG_BT_MAX_CONN];

	struct bt_bap_bass_subgroup subgroups[CONFIG_BT_BAP_BASS_MAX_SUBGROUPS];
	struct bt_cap_commander_broadcast_reception_start_member_param
		start_member_params[CONFIG_BT_MAX_CONN];
	struct bt_cap_commander_broadcast_reception_start_param start_param;
	struct bt_cap_commander_broadcast_reception_stop_member_param
		stop_member_params[CONFIG_BT_MAX_CONN];
	struct bt_cap_commander_broadcast_reception_stop_param stop_param;
	struct bt_bap_broadcast_assistant_cb broadcast_assistant_cb;
};

/* src_id can not be part of the fixture since it is accessed in the callback function */
static uint8_t src_id[CONFIG_BT_MAX_CONN];

static void test_start_param_init(void *f);
static void test_stop_param_init(void *f);

static void cap_commander_broadcast_assistant_recv_state_cb(
	struct bt_conn *conn, int err, const struct bt_bap_scan_delegator_recv_state *state)
{
	uint8_t index;

	ARG_UNUSED(err);

	index = bt_conn_index(conn);
	src_id[index] = state->src_id;
}

static void cap_commander_test_broadcast_reception_fixture_init(
	struct cap_commander_test_broadcast_reception_fixture *fixture)
{
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		test_conn_init(&fixture->conns[i]);
		fixture->conns[i].index = i;
	}
	test_start_param_init(fixture);
	test_stop_param_init(fixture);

	fixture->broadcast_assistant_cb.recv_state =
		cap_commander_broadcast_assistant_recv_state_cb;
	err = bt_bap_broadcast_assistant_register_cb(&fixture->broadcast_assistant_cb);
	assert_int_equal(0, err);
}

static void *cap_commander_test_broadcast_reception_setup(void)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_commander_test_broadcast_reception_before(void *f)
{
	int err;
	struct cap_commander_test_broadcast_reception_fixture *fixture = f;

	memset(f, 0, sizeof(struct cap_commander_test_broadcast_reception_fixture));
	cap_commander_test_broadcast_reception_fixture_init(fixture);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		err = bt_cap_commander_discover(&fixture->conns[i]);
		assert_int_equal(0, err);
	}
}

static void cap_commander_test_broadcast_reception_after(void *f)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = f;
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

static void cap_commander_test_broadcast_reception_teardown(void *f)
{
	free(f);
}

static void test_start_param_init(void *f)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = f;
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
		fixture->start_member_params[i].adv_sid = SID;
		fixture->start_member_params[i].pa_interval = ADV_INTERVAL;
		fixture->start_member_params[i].broadcast_id = BROADCAST_ID;
		memcpy(fixture->start_member_params[i].subgroups, &fixture->subgroups[0],
		       sizeof(struct bt_bap_bass_subgroup) * CONFIG_BT_BAP_BASS_MAX_SUBGROUPS);
		fixture->start_member_params[i].num_subgroups = CONFIG_BT_BAP_BASS_MAX_SUBGROUPS;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		err = bt_cap_commander_discover(&fixture->conns[i]);
		assert_int_equal(0, err);
	}
}

static void test_stop_param_init(void *f)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = f;

	fixture->stop_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->stop_param.param = fixture->stop_member_params;
	fixture->stop_param.count = ARRAY_SIZE(fixture->stop_member_params);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->stop_member_params); i++) {
		fixture->stop_member_params[i].member.member = &fixture->conns[i];
		fixture->stop_member_params[i].src_id = 0U;
		fixture->stop_member_params[i].num_subgroups = CONFIG_BT_BAP_BASS_MAX_SUBGROUPS;
	}
}

static void
test_broadcast_reception_start(struct bt_cap_commander_broadcast_reception_start_param *start_param)
{
	int err;

	err = bt_cap_commander_broadcast_reception_start(start_param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 1,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
	assert_ptr_equal(NULL, mock_cap_commander_broadcast_reception_start_cb_fake.arg0_history[0]);
	assert_int_equal(0, mock_cap_commander_broadcast_reception_start_cb_fake.arg1_history[0]);
}

static void
test_broadcast_reception_stop(struct bt_cap_commander_broadcast_reception_stop_param *stop_param)
{
	int err;

	err = bt_cap_commander_broadcast_reception_stop(stop_param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 1,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
	assert_ptr_equal(NULL, mock_cap_commander_broadcast_reception_stop_cb_fake.arg0_history[0]);
	assert_int_equal(0, mock_cap_commander_broadcast_reception_stop_cb_fake.arg1_history[0]);
}



static void cap_commander_test_broadcast_reception__test_commander_reception_start(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_broadcast_reception_start(&fixture->start_param);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_one_subgroup(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	/* We test with one subgroup, instead of CONFIG_BT_BAP_BASS_MAX_SUBGROUPS subgroups */
	for (size_t i = 0U; i < CONFIG_BT_MAX_CONN; i++) {
		fixture->start_param.param[i].num_subgroups = 1U;
	}

	test_broadcast_reception_start(&fixture->start_param);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_double(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_broadcast_reception_start(&fixture->start_param);

	/*
	 * We can not use test_broadcast_reception_start because of the check on how often the
	 * callback function is called
	 */
	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 2,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_null(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_broadcast_reception_start(NULL);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_zero_count(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.count = 0;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_high_count(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.count = CONFIG_BT_MAX_CONN + 1;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_null_param(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->start_param.param = NULL;
	fixture->start_param.count = ARRAY_SIZE(fixture->conns);

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_null_member(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].member.member = NULL;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_missing_cas(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.type = BT_CAP_SET_TYPE_CSIP;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_addr_type(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].addr.type = BT_ADDR_LE_RANDOM + 1;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_sid(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].adv_sid = BT_GAP_SID_MAX + 1;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_pa_interval_low(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].pa_interval = BT_GAP_PER_ADV_MIN_INTERVAL - 1;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

/*
 * Test for pa_interval_high omitted
 * pa_interval is a uint16_t, BT_GAP_PER_ADV_MAX_INTERVAL is defined as 0xFFFF
 * and therefore we can not test in the current implementation
 */

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_broadcast_id(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].broadcast_id = BT_AUDIO_BROADCAST_ID_MAX + 1;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_no_subgroups(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].num_subgroups = 0;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_num_subgroups(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->start_param.param[0].num_subgroups = CONFIG_BT_BAP_BASS_MAX_SUBGROUPS + 1;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_duplicate_bis_sync(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	if (CONFIG_BT_BAP_BASS_MAX_SUBGROUPS == 1) {
		skip();
	}

	fixture->start_param.param[0].subgroups[0].bis_sync =
		fixture->start_param.param[0].subgroups[1].bis_sync;

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_start_inval_metadata_len(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	if (CONFIG_BT_AUDIO_CODEC_CFG_MAX_METADATA_SIZE >= UINT8_MAX) {
		skip();
	}

	fixture->start_param.param[0].subgroups[0].metadata_len =
		(uint8_t)(CONFIG_BT_AUDIO_CODEC_CFG_MAX_METADATA_SIZE + 1);

	err = bt_cap_commander_broadcast_reception_start(&fixture->start_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_start", 0,
			   mock_cap_commander_broadcast_reception_start_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_default_subgroups(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_broadcast_reception_start(&fixture->start_param);

	for (size_t i = 0U; i < CONFIG_BT_MAX_CONN; i++) {
		fixture->stop_param.param[i].src_id = src_id[i];
	}

	test_broadcast_reception_stop(&fixture->stop_param);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_one_subgroup(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_broadcast_reception_start(&fixture->start_param);

	/* We test with one subgroup, instead of CONFIG_BT_BAP_BASS_MAX_SUBGROUPS subgroups */
	for (size_t i = 0U; i < CONFIG_BT_MAX_CONN; i++) {
		fixture->stop_param.param[i].num_subgroups = 1U;
		fixture->stop_param.param[i].src_id = src_id[i];
	}

	test_broadcast_reception_stop(&fixture->stop_param);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_double(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	test_broadcast_reception_start(&fixture->start_param);

	for (size_t i = 0U; i < CONFIG_BT_MAX_CONN; i++) {
		bt_log_print("Source ID %d: %d %d\n", i, fixture->stop_param.param[i].src_id, src_id[i]);

		fixture->stop_param.param[i].src_id = src_id[i];
	}

	test_broadcast_reception_stop(&fixture->stop_param);

	/*
	 * We can not use test_broadcast_reception_stop because of the check on how often the
	 * callback function is called
	 */
	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(0, err);
	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 2,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_null(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_broadcast_reception_stop(NULL);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_zero_count(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.count = 0;

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_high_count(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.count = CONFIG_BT_MAX_CONN + 1;

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_null_param(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->stop_param.param = NULL;
	fixture->stop_param.count = ARRAY_SIZE(fixture->conns);

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_null_member(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.param[0].member.member = NULL;

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_missing_cas(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.type = BT_CAP_SET_TYPE_CSIP;

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_no_subgroups(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.param[0].num_subgroups = 0;

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

static void cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_num_subgroups(void **cm_state)
{
	struct cap_commander_test_broadcast_reception_fixture *fixture = *cm_state;
	int err;

	fixture->stop_param.param[0].num_subgroups = CONFIG_BT_BAP_BASS_MAX_SUBGROUPS + 1;

	err = bt_cap_commander_broadcast_reception_stop(&fixture->stop_param);
	assert_int_equal(-EINVAL, err);

	zexpect_call_count("bt_cap_commander_cb.broadcast_reception_stop", 0,
			   mock_cap_commander_broadcast_reception_stop_cb_fake.call_count);
}

int bt_test_cap_commander_test_broadcast_reception_setup(void **state)
{
	int environment_result = bt_test_audio_cap_commander_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_commander_test_broadcast_reception_setup();
	return 0;
}
static int bt_test_cap_commander_test_broadcast_reception_before(void **state)
{
	(void)state;
	cap_commander_test_broadcast_reception_before(*state);
	mock_init_rule_before_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_broadcast_reception_after(void **state)
{
	(void)state;
	cap_commander_test_broadcast_reception_after(*state);
	mock_destroy_rule_after_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_broadcast_reception_teardown(void **state)
{
	(void)state;
	cap_commander_test_broadcast_reception_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_commander_test_broadcast_reception[] = {
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_one_subgroup, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_double, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_null, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_zero_count, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_high_count, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_param_null_param, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_null_member, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_missing_cas, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_addr_type, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_sid, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_pa_interval_low, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_broadcast_id, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_no_subgroups, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_num_subgroups, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_duplicate_bis_sync, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_start_inval_metadata_len, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_default_subgroups, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_one_subgroup, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_double, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_null, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_zero_count, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_high_count, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_param_null_param, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_null_member, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_missing_cas, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_no_subgroups, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_broadcast_reception__test_commander_reception_stop_inval_num_subgroups, bt_test_cap_commander_test_broadcast_reception_before, bt_test_cap_commander_test_broadcast_reception_after),
};
BT_TEST_SUITE(cap_commander_test_broadcast_reception, test_cases_cap_commander_test_broadcast_reception, bt_test_cap_commander_test_broadcast_reception_setup, bt_test_cap_commander_test_broadcast_reception_teardown);
