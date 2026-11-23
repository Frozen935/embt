#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_744017(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_744017(const struct bt_test_case_info *, void *);
void mock_init_rule_before_127f12(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_127f12(const struct bt_test_case_info *, void *);

void mock_init_rule_before_127f12(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_127f12(const struct bt_test_case_info *, void *);
void mock_init_rule_before_127f12(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_127f12(const struct bt_test_case_info *, void *);

/* main.c - Application main entry point */

/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/assigned_numbers.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/bap_lc3_preset.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/byteorder.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/iso.h>
#include <fff.h>
#include <embt/base/byteorder.h>
#include <embt/base/util.h>
#include <embt/compiler.h>
#include "audio/bap_endpoint.h"
#include "audio/bap_iso.h"
#include "bluetooth.h"
#include "cap_initiator.h"
#include "cap_handover.h"
#include "conn.h"
#include "expects_util.h"
#include "test_common.h"

void mock_init_rule_before_127f12(const struct bt_test_case_info *test, void *fixture)
{
	test_mocks_init();
}

void mock_destroy_rule_after_127f12(const struct bt_test_case_info *test, void *fixture)
{
	test_mocks_cleanup();
}



/* Either BT_AUDIO_DIR_SINK or BT_AUDIO_DIR_SOURCE */
#define INDEX_TO_DIR(_idx)    (((_idx) & 1U) + 1U)
#define STREAMS_PER_DIRECTION 2
#define MAX_STREAMS           4
BUILD_ASSERT(MAX_STREAMS >= STREAMS_PER_DIRECTION);
BUILD_ASSERT(CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT >= MAX_STREAMS);
BUILD_ASSERT(CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT >= STREAMS_PER_DIRECTION);

struct cap_handover_unicast_to_broadcast_test_suite_fixture {
	struct bt_cap_unicast_audio_start_stream_param
		unicast_audio_start_stream_params[MAX_STREAMS];
	struct bt_cap_initiator_broadcast_stream_param
		broadcast_stream_params[STREAMS_PER_DIRECTION];
	struct bt_bap_ep *snk_eps[CONFIG_BT_MAX_CONN][CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT];
	struct bt_bap_ep *src_eps[CONFIG_BT_MAX_CONN][CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT];
	struct bt_cap_handover_unicast_to_broadcast_param unicast_to_broadcast_param;
	struct bt_cap_initiator_broadcast_create_param broadcast_create_param;
	struct bt_cap_unicast_audio_start_param unicast_audio_start_param;
	struct bt_cap_initiator_broadcast_subgroup_param subgroup_params;
	struct bt_bap_lc3_preset unicast_presets[MAX_STREAMS];
	struct bt_cap_stream cap_streams[MAX_STREAMS];
	struct bt_cap_unicast_group *unicast_group;
	struct bt_bap_lc3_preset broadcast_preset;
	struct bt_conn conns[CONFIG_BT_MAX_CONN];
	struct bt_le_ext_adv ext_adv;
};

static void *cap_handover_unicast_to_broadcast_test_suite_setup(void)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_handover_unicast_to_broadcast_test_suite_before(void *f)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = f;
	struct bt_cap_unicast_group_stream_pair_param
		unicast_group_stream_pair_params[MAX_STREAMS / STREAMS_PER_DIRECTION] = {0};
	struct bt_cap_unicast_group_stream_param unicast_group_stream_params[MAX_STREAMS] = {0};
	struct bt_cap_unicast_group_param group_param = {0};
	size_t stream_cnt = 0U;
	size_t pair_cnt = 0U;
	int err;

	memset(fixture, 0, sizeof(*fixture));

	err = bt_cap_initiator_register_cb(&mock_cap_initiator_cb);
	assert_int_equal(0, err);

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);

	/* Create unicast group */
	ARRAY_FOR_EACH(fixture->unicast_presets, i) {
		fixture->unicast_presets[i] =
			(struct bt_bap_lc3_preset)BT_BAP_LC3_UNICAST_PRESET_16_2_1(
				BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	}

	ARRAY_FOR_EACH(fixture->conns, i) {
		test_conn_init(&fixture->conns[i], i);

		mock_unicast_client_discover(&fixture->conns[i], fixture->snk_eps[i],
					     fixture->src_eps[i]);
	}

	while (stream_cnt < ARRAY_SIZE(unicast_group_stream_params)) {
		const enum bt_audio_dir dir = INDEX_TO_DIR(stream_cnt);

		unicast_group_stream_params[stream_cnt].stream = &fixture->cap_streams[stream_cnt];
		unicast_group_stream_params[stream_cnt].qos_cfg =
			&fixture->unicast_presets[stream_cnt].qos;

		/* Switch between sink and source depending on index*/
		if (dir == BT_AUDIO_DIR_SINK) {
			unicast_group_stream_pair_params[pair_cnt].tx_param =
				&unicast_group_stream_params[stream_cnt];
		} else {
			unicast_group_stream_pair_params[pair_cnt].rx_param =
				&unicast_group_stream_params[stream_cnt];
		}

		pair_cnt = DIV_ROUND_UP(stream_cnt, 2U);
		stream_cnt++;
	}

	group_param.packing = BT_ISO_PACKING_SEQUENTIAL;
	group_param.params_count = pair_cnt;
	group_param.params = unicast_group_stream_pair_params;

	err = bt_cap_unicast_group_create(&group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	/* Start unicast group */
	fixture->unicast_audio_start_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->unicast_audio_start_param.count =
		ARRAY_SIZE(fixture->unicast_audio_start_stream_params);
	fixture->unicast_audio_start_param.stream_params =
		fixture->unicast_audio_start_stream_params;

	ARRAY_FOR_EACH(fixture->unicast_audio_start_stream_params, i) {
		struct bt_cap_unicast_audio_start_stream_param *stream_param =
			&fixture->unicast_audio_start_stream_params[i];

		/* We pair 2 streams, so only increase conn_index every 2nd stream and otherwise
		 * round robin on all conns
		 */
		const size_t conn_index = (i / 2U) % ARRAY_SIZE(fixture->conns);
		const size_t ep_index = i / (ARRAY_SIZE(fixture->conns) * 2U);
		const enum bt_audio_dir dir = INDEX_TO_DIR(i);

		stream_param->stream = &fixture->cap_streams[i];
		stream_param->codec_cfg = &fixture->unicast_presets[i].codec_cfg;

		/* Distribute the streams like
		 * [0]: conn[0] src[0]
		 * [1]: conn[0] snk[0]
		 * [2]: conn[1] src[0]
		 * [3]: conn[0] snk[0]
		 * [4]: conn[0] src[1]
		 * [5]: conn[0] snk[1]
		 * [6]: conn[1] src[1]
		 * [7]: conn[1] snk[1]
		 */
		stream_param->member.member = &fixture->conns[conn_index];
		if (dir == BT_AUDIO_DIR_SINK) {
			stream_param->ep = fixture->snk_eps[conn_index][ep_index];
		} else {
			stream_param->ep = fixture->src_eps[conn_index][ep_index];
		}
	}

	err = bt_cap_initiator_unicast_audio_start(&fixture->unicast_audio_start_param);
	assert_int_equal(err, 0);

	zexpect_call_count("bt_cap_initiator_cb.unicast_start_complete_cb", 1,
			   mock_unicast_start_complete_cb_fake.call_count);
	assert_int_equal(0, mock_unicast_start_complete_cb_fake.arg0_history[0]);
	assert_ptr_equal(NULL, mock_unicast_start_complete_cb_fake.arg1_history[0]);

	/* Prepare default handover parameters including broadcast source create parameters */
	fixture->ext_adv.ext_adv_state = BT_LE_EXT_ADV_STATE_ENABLED;
	fixture->ext_adv.per_adv_state = BT_LE_PER_ADV_STATE_ENABLED;

	fixture->broadcast_preset = (struct bt_bap_lc3_preset)BT_BAP_LC3_BROADCAST_PRESET_16_2_1(
		BT_AUDIO_LOCATION_MONO_AUDIO, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	fixture->unicast_to_broadcast_param.type = BT_CAP_SET_TYPE_AD_HOC;
	fixture->unicast_to_broadcast_param.ext_adv = &fixture->ext_adv;
	fixture->unicast_to_broadcast_param.unicast_group = fixture->unicast_group;
	fixture->unicast_to_broadcast_param.pa_interval = 0x1234U;
	fixture->unicast_to_broadcast_param.broadcast_id = TEST_COMMON_BROADCAST_ID;
	fixture->unicast_to_broadcast_param.broadcast_create_param =
		&fixture->broadcast_create_param;

	fixture->broadcast_create_param.subgroup_count = 1U;
	fixture->broadcast_create_param.subgroup_params = &fixture->subgroup_params;
	fixture->broadcast_create_param.qos = &fixture->broadcast_preset.qos;
	fixture->broadcast_create_param.packing = BT_ISO_PACKING_SEQUENTIAL;
	fixture->broadcast_create_param.encryption = false;

	/* We use pair_cnt as stream_count as it is equal to the number of sink streams */
	fixture->subgroup_params.stream_count = pair_cnt;
	fixture->subgroup_params.stream_params = fixture->broadcast_stream_params;
	fixture->subgroup_params.codec_cfg = &fixture->broadcast_preset.codec_cfg;

	for (size_t i = 0U; i < pair_cnt; i++) {
		fixture->broadcast_stream_params[i].stream =
			unicast_group_stream_pair_params[i].tx_param->stream;
	}
}

static void cap_handover_unicast_to_broadcast_test_suite_after(void *f)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = f;
	int err;

	err = bt_cap_initiator_unregister_cb(&mock_cap_initiator_cb);
	assert_true(err == 0 || err == -EINVAL);

	err = bt_cap_handover_unregister_cb(&mock_cap_handover_cb);
	assert_true(err == 0 || err == -EINVAL);

	ARRAY_FOR_EACH_PTR(fixture->conns, conn) {
		mock_bt_conn_disconnected(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}

	/* In the case of a test failing, we cancel the procedure so that subsequent won't fail */
	err = bt_cap_initiator_unicast_audio_cancel();
	/* May fail if no CAP procedure is in progress */
	assert_true(err == 0 || err == -EALREADY);

	/* In the case of a test failing, we delete the group so that subsequent tests won't fail */
	if (fixture->unicast_group != NULL) {
		struct bt_cap_stream *cap_stream_ptrs[MAX_STREAMS];

		const struct bt_cap_unicast_audio_stop_param param = {
			.type = BT_CAP_SET_TYPE_AD_HOC,
			.count = ARRAY_SIZE(fixture->cap_streams),
			.streams = cap_stream_ptrs,
			.release = true,
		};

		ARRAY_FOR_EACH(cap_stream_ptrs, idx) {
			cap_stream_ptrs[idx] = &fixture->cap_streams[idx];
		}

		err = bt_cap_initiator_unicast_audio_stop(&param);
		assert_true(err == 0 || err == -EALREADY);

		err = bt_cap_unicast_group_delete(fixture->unicast_group);
		assert_true(err == 0);
		fixture->unicast_group = NULL;
	}

	/* If a broadcast source was create it exists as the 4th parameter in the callback */
	if (mock_unicast_to_broadcast_complete_cb_fake.arg3_history[0] != NULL) {
		struct bt_cap_broadcast_source *broadcast_source =
			mock_unicast_to_broadcast_complete_cb_fake.arg3_history[0];

		err = bt_cap_initiator_broadcast_audio_stop(broadcast_source);
		assert_true(err == 0 || err == -EALREADY || err == -EBADMSG);
		err = bt_cap_initiator_broadcast_audio_delete(broadcast_source);
		assert_true(err == 0);
	}
}

static void cap_handover_unicast_to_broadcast_test_suite_teardown(void *f)
{
	free(f);
}



static void validate_handover_callback(void)
{
	zexpect_call_count("bt_cap_initiator_cb.unicast_to_broadcast_complete_cb", 1,
			   mock_unicast_to_broadcast_complete_cb_fake.call_count);
	assert_int_equal(0, mock_unicast_to_broadcast_complete_cb_fake.arg0_history[0]);
	assert_ptr_equal(NULL, mock_unicast_to_broadcast_complete_cb_fake.arg1_history[0]);
	assert_ptr_equal(NULL, mock_unicast_to_broadcast_complete_cb_fake.arg2_history[0]);
	assert_int_not_equal(NULL, mock_unicast_to_broadcast_complete_cb_fake.arg3_history[0]);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, 0);
	validate_handover_callback();
	fixture->unicast_group = NULL;
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inactive_adv(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.ext_adv->ext_adv_state = BT_LE_EXT_ADV_STATE_DISABLED;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, 0);
	fixture->unicast_group = NULL;
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_param(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_handover_unicast_to_broadcast(NULL);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_unicast_group(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.unicast_group = NULL;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_ext_adv(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.ext_adv = NULL;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_ext_adv_no_pa(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.ext_adv->per_adv_state = BT_LE_PER_ADV_STATE_NONE;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_pa_interval(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.pa_interval = 0U;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_broadcast_id(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.broadcast_id = 0xFFFFFFFFU;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_broadcast_create_param(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	fixture->unicast_to_broadcast_param.broadcast_create_param = NULL;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_broadcast_stream(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	struct bt_cap_stream cap_stream = {0};
	int err = 0;

	/* Attempt to use a stream not in the unicast group */
	fixture->broadcast_stream_params[0].stream = &cap_stream;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_broadcast_stream_cnt(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;

	if (fixture->subgroup_params.stream_count == 1U) {
		skip();
	}

	/* Attempt to not convert all sink streams */
	fixture->subgroup_params.stream_count--;

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_no_active_sink_streams(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	struct bt_cap_stream *cap_stream_ptrs[MAX_STREAMS];
	const struct bt_cap_unicast_audio_stop_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.count = fixture->subgroup_params.stream_count,
		.streams = cap_stream_ptrs,
		.release = false,
	};
	int err = 0;

	for (size_t i = 0; i < param.count; i++) {
		param.streams[i] = fixture->broadcast_stream_params[i].stream;
	}

	ARRAY_FOR_EACH(cap_stream_ptrs, idx) {
		cap_stream_ptrs[idx] = &fixture->cap_streams[idx];
	}

	/* Test that it will fail if there are no active sink streams */
	err = bt_cap_initiator_unicast_audio_stop(&param);
	assert_int_equal(err, 0);

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_unique_metadata(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	struct bt_cap_stream *cap_stream_ptrs[MAX_STREAMS];
	const struct bt_cap_unicast_audio_stop_param param = {
		.type = BT_CAP_SET_TYPE_AD_HOC,
		.count = fixture->unicast_audio_start_param.count,
		.streams = cap_stream_ptrs,
		.release = true,
	};
	int err = 0;

	if (STREAMS_PER_DIRECTION <= 1U) {
		skip();
	}

	ARRAY_FOR_EACH(cap_stream_ptrs, idx) {
		cap_stream_ptrs[idx] = &fixture->cap_streams[idx];
	}

	/* Stop so that we can change the metadata  */
	err = bt_cap_initiator_unicast_audio_stop(&param);
	assert_int_equal(err, 0);

	/* Make metadata unique per stream to require additional subgroups */
	ARRAY_FOR_EACH(fixture->cap_streams, i) {
		fixture->unicast_presets[i].codec_cfg.meta[0] = 3U; /* length */
		fixture->unicast_presets[i].codec_cfg.meta[1] =
			BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT;                        /* type */
		sys_put_le16(i + 1U, &fixture->unicast_presets[i].codec_cfg.meta[2]); /* value */
		fixture->unicast_presets[i].codec_cfg.meta_len = 4U;
	}

	/* Restart with unique metadata */
	err = bt_cap_initiator_unicast_audio_start(&fixture->unicast_audio_start_param);
	assert_int_equal(err, 0);

	/* Attempt handover with unique metadata to trigger subgroup count > max */
	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_unicast_group(void **cm_state)
{
	struct cap_handover_unicast_to_broadcast_test_suite_fixture *fixture = *cm_state;
	int err = 0;
	void *group = fixture->broadcast_stream_params[0].stream->bap_stream.group;

	/* Attempt to use a stream with invalid unicast group */
	fixture->broadcast_stream_params[0].stream->bap_stream.group = UINT_TO_POINTER(0x12345678U);

	err = bt_cap_handover_unicast_to_broadcast(&fixture->unicast_to_broadcast_param);
	assert_int_equal(err, -EINVAL);

	/* Restore group to support proper cleanup after the test */
	fixture->broadcast_stream_params[0].stream->bap_stream.group = group;
}

int bt_test_cap_handover_unicast_to_broadcast_test_suite_setup(void **state)
{
	int environment_result = bt_test_audio_cap_handover_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_handover_unicast_to_broadcast_test_suite_setup();
	return 0;
}
static int bt_test_cap_handover_unicast_to_broadcast_test_suite_before(void **state)
{
	(void)state;
	cap_handover_unicast_to_broadcast_test_suite_before(*state);
	mock_init_rule_before_744017(NULL, *state);
	mock_init_rule_before_127f12(NULL, *state);
	return 0;
}
int bt_test_cap_handover_unicast_to_broadcast_test_suite_after(void **state)
{
	(void)state;
	cap_handover_unicast_to_broadcast_test_suite_after(*state);
	mock_destroy_rule_after_744017(NULL, *state);
	mock_destroy_rule_after_127f12(NULL, *state);
	return 0;
}
int bt_test_cap_handover_unicast_to_broadcast_test_suite_teardown(void **state)
{
	(void)state;
	cap_handover_unicast_to_broadcast_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_handover_unicast_to_broadcast_test_suite[] = {
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inactive_adv, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_param, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_unicast_group, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_ext_adv, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_ext_adv_no_pa, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_pa_interval, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_broadcast_id, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_null_broadcast_create_param, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_broadcast_stream, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_broadcast_stream_cnt, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_no_active_sink_streams, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_unique_metadata, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
	cmocka_unit_test_setup_teardown(cap_handover_unicast_to_broadcast_test_suite__test_handover_unicast_to_broadcast_inval_unicast_group, bt_test_cap_handover_unicast_to_broadcast_test_suite_before, bt_test_cap_handover_unicast_to_broadcast_test_suite_after),
};
BT_TEST_SUITE(cap_handover_unicast_to_broadcast_test_suite, test_cases_cap_handover_unicast_to_broadcast_test_suite, bt_test_cap_handover_unicast_to_broadcast_test_suite_setup, bt_test_cap_handover_unicast_to_broadcast_test_suite_teardown);
