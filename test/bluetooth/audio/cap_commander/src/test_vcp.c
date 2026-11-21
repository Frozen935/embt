#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_d38d7b(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_d38d7b(const struct bt_test_case_info *, void *);


/* test_vcp.c - unit test for volume settings */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/audio/vcp.h>
#include <embt/bluetooth/audio/vocs.h>
#include <embt/bluetooth/hci_types.h>
#include <fff.h>
#include <embt/base/util.h>
#include "cap_commander.h"
#include "conn.h"
#include "expects_util.h"
#include "cap_mocks.h"
#include "test_common.h"

#define FFF_GLOBALS

struct cap_commander_test_vcp_fixture {
	struct bt_conn conns[CONFIG_BT_MAX_CONN];
};

static void cap_commander_test_vcp_fixture_init(struct cap_commander_test_vcp_fixture *fixture)
{
	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		test_conn_init(&fixture->conns[i]);
	}
}

static void *cap_commander_test_vcp_setup(void)
{
	struct cap_commander_test_vcp_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_commander_test_vcp_before(void *f)
{
	memset(f, 0, sizeof(struct cap_commander_test_vcp_fixture));
	cap_commander_test_vcp_fixture_init(f);
}

static void cap_commander_test_vcp_after(void *f)
{
	struct cap_commander_test_vcp_fixture *fixture = f;
	int err;

	err = bt_cap_commander_unregister_cb(&mock_cap_commander_cb);
	assert_true(err == 0 || err == -EINVAL);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		mock_bt_conn_disconnected(&fixture->conns[i], BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
}

static void cap_commander_test_vcp_teardown(void *f)
{
	free(f);
}



static void cap_commander_test_vcp__test_commander_change_volume(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_changed", 1,
			   mock_cap_commander_volume_changed_cb_fake.call_count);
}

static void cap_commander_test_vcp__test_commander_change_volume_double(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_changed", 1,
			   mock_cap_commander_volume_changed_cb_fake.call_count);

	/* Verify that it still works as expected if we set the same value twice */
	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_changed", 2,
			   mock_cap_commander_volume_changed_cb_fake.call_count);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_param_null(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_change_volume(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_param_null_members(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = NULL,
		.count = ARRAY_SIZE(fixture->conns),
		.volume = 177,
	};
	int err;

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_param_null_member(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members) - 1; i++) {
		members[i].member = &fixture->conns[i];
	}
	members[ARRAY_SIZE(members) - 1].member = NULL;

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_missing_cas(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_CSIP,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_missing_vcs(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_param_zero_count(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = 0U,
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_inval_param_inval_count(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = CONFIG_BT_MAX_CONN + 1,
		.volume = 177,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_change_volume(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_offset_changed", 1,
			   mock_cap_commander_volume_offset_changed_cb_fake.call_count);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_double(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_offset_changed", 1,
			   mock_cap_commander_volume_offset_changed_cb_fake.call_count);

	/* Verify that it still works as expected if we set the same value twice */
	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_offset_changed", 2,
			   mock_cap_commander_volume_offset_changed_cb_fake.call_count);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_null(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_change_volume_offset(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_null_param(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = NULL,
		.count = ARRAY_SIZE(fixture->conns),
	};
	int err;

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_null_member(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}
	member_params[ARRAY_SIZE(member_params) - 1].member.member = NULL;

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_missing_cas(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_CSIP,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_missing_vocs(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_zero_count(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = 0U,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_inval_count(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = CONFIG_BT_MAX_CONN + 1,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = 100 + i;
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_inval_offset_max(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = BT_VOCS_MAX_OFFSET + 1;
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_inval_offset_min(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	struct bt_cap_commander_change_volume_offset_member_param
		member_params[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_offset_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.param = member_params,
		.count = ARRAY_SIZE(member_params),
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(member_params); i++) {
		member_params[i].member.member = &fixture->conns[i];
		member_params[i].offset = BT_VOCS_MIN_OFFSET - 1;
	}

	err = bt_cap_commander_change_volume_offset(&param);
	assert_int_equal(-EINVAL, err);
}
static void cap_commander_test_vcp__test_commander_change_volume_mute_state(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_mute_changed", 1,
			   mock_cap_commander_volume_mute_changed_cb_fake.call_count);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_double(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_mute_changed", 1,
			   mock_cap_commander_volume_mute_changed_cb_fake.call_count);

	/* Verify that it still works as expected if we set the same value twice */
	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(0, err);

	zexpect_call_count("bt_cap_commander_cb.volume_mute_changed", 2,
			   mock_cap_commander_volume_mute_changed_cb_fake.call_count);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_null(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_commander_change_volume_mute_state(NULL);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_null_members(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = NULL,
		.count = ARRAY_SIZE(fixture->conns),
		.mute = true,
	};
	int err;

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_null_member(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members) - 1; i++) {
		members[i].member = &fixture->conns[i];
	}
	members[ARRAY_SIZE(members) - 1].member = NULL;

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_missing_cas(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_CSIP,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	for (size_t i = 0U; i < ARRAY_SIZE(fixture->conns); i++) {
		struct bt_vcp_vol_ctlr *vol_ctlr; /* We don't care about this */

		err = bt_vcp_vol_ctlr_discover(&fixture->conns[i], &vol_ctlr);
		assert_int_equal(0, err);
	}

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_missing_vcs(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = ARRAY_SIZE(fixture->conns),
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_register_cb(&mock_cap_commander_cb);
	assert_int_equal(0, err);

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_zero_count(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = 0U,
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(-EINVAL, err);
}

static void cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_inval_count(void **cm_state)
{
	struct cap_commander_test_vcp_fixture *fixture = *cm_state;
	union bt_cap_set_member members[ARRAY_SIZE(fixture->conns)];
	const struct bt_cap_commander_change_volume_mute_state_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.members = members,
		.count = CONFIG_BT_MAX_CONN + 1,
		.mute = true,
	};
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(members); i++) {
		members[i].member = &fixture->conns[i];
	}

	err = bt_cap_commander_change_volume_mute_state(&param);
	assert_int_equal(-EINVAL, err);
}

int bt_test_cap_commander_test_vcp_setup(void **state)
{
	int environment_result = bt_test_audio_cap_commander_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_commander_test_vcp_setup();
	return 0;
}
static int bt_test_cap_commander_test_vcp_before(void **state)
{
	(void)state;
	cap_commander_test_vcp_before(*state);
	mock_init_rule_before_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_vcp_after(void **state)
{
	(void)state;
	cap_commander_test_vcp_after(*state);
	mock_destroy_rule_after_d38d7b(NULL, *state);
	return 0;
}
int bt_test_cap_commander_test_vcp_teardown(void **state)
{
	(void)state;
	cap_commander_test_vcp_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_commander_test_vcp[] = {
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_double, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_param_null, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_param_null_members, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_param_null_member, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_missing_cas, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_missing_vcs, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_param_zero_count, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_inval_param_inval_count, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_double, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_null, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_null_param, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_null_member, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_missing_cas, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_missing_vocs, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_zero_count, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_inval_count, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_inval_offset_max, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_offset_inval_param_inval_offset_min, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_double, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_null, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_null_members, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_null_member, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_missing_cas, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_missing_vcs, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_zero_count, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
	cmocka_unit_test_setup_teardown(cap_commander_test_vcp__test_commander_change_volume_mute_state_inval_param_inval_count, bt_test_cap_commander_test_vcp_before, bt_test_cap_commander_test_vcp_after),
};
BT_TEST_SUITE(cap_commander_test_vcp, test_cases_cap_commander_test_vcp, bt_test_cap_commander_test_vcp_setup, bt_test_cap_commander_test_vcp_teardown);
