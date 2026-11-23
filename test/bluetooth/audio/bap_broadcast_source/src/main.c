#include "support/test_support.h"
void mock_init_rule_before_a2543a(const struct bt_test_case_info *, void *);
void mock_init_rule_before_17cc08(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_17cc08(const struct bt_test_case_info *, void *);

void mock_init_rule_before_17cc08(const struct bt_test_case_info *, void *);
void mock_init_rule_before_17cc08(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_17cc08(const struct bt_test_case_info *, void *);

/* main.c - Application main entry point */

/*
 * Copyright (c) 2023-2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <embt/bluetooth/assigned_numbers.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/lc3.h>
#include <embt/bluetooth/byteorder.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/iso.h>
#include <fff.h>

#include <embt/net_buf.h>
#include <embt/base/log.h>
#include <embt/base/util_macro.h>
#include <embt/compiler.h>
#include "bap_broadcast_source.h"
#include "bap_stream.h"
#include "bluetooth.h"
#include "expects_util.h"

DEFINE_FFF_GLOBALS;

void mock_init_rule_before_17cc08(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	mock_bap_broadcast_source_init();
	mock_bap_stream_init();
}

void mock_destroy_rule_after_17cc08(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	mock_bap_stream_cleanup();
}



struct bap_broadcast_source_test_suite_fixture {
	struct bt_bap_broadcast_source_param *param;
	struct bt_audio_codec_cfg *codec_cfg;
	struct bt_bap_qos_cfg *qos_cfg;
	uint8_t *bis_data;
	size_t stream_cnt;
	struct bt_bap_broadcast_source *source;
};

static void bap_broadcast_source_test_suite_fixture_init(
	struct bap_broadcast_source_test_suite_fixture *fixture)
{
	const uint8_t bis_cfg_data[] = {
		BT_AUDIO_CODEC_DATA(BT_AUDIO_CODEC_CFG_CHAN_ALLOC,
				    BT_BYTES_LIST_LE32(BT_AUDIO_LOCATION_FRONT_LEFT |
						       BT_AUDIO_LOCATION_FRONT_RIGHT)),
	};
	const size_t streams_per_subgroup = CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT /
					    CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT;
	const enum bt_audio_context ctx = BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED;
	const enum bt_audio_location loc = BT_AUDIO_LOCATION_FRONT_LEFT;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_param;
	struct bt_bap_broadcast_source_stream_param *stream_params;
	struct bt_bap_stream *streams;
	const uint16_t latency = 10U; /* ms*/
	const uint32_t pd = 40000U;   /* us */
	const uint16_t sdu = 40U;     /* octets */
	const uint8_t rtn = 2U;

	assert_true(streams_per_subgroup > 0U);
	assert_true(sizeof(bis_cfg_data) <= CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE);

	/* Allocate memory for everything */
	fixture->param = malloc(sizeof(struct bt_bap_broadcast_source_param));
	subgroup_param = malloc(sizeof(struct bt_bap_broadcast_source_subgroup_param) *
				CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT);
	assert_non_null(subgroup_param);
	stream_params = malloc(sizeof(struct bt_bap_broadcast_source_stream_param) *
			       CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT);
	assert_non_null(stream_params);
	fixture->codec_cfg = malloc(sizeof(struct bt_audio_codec_cfg));
	assert_non_null(fixture->codec_cfg);
	fixture->qos_cfg = malloc(sizeof(struct bt_bap_qos_cfg));
	assert_non_null(fixture->qos_cfg);
	streams = malloc(sizeof(struct bt_bap_stream) * CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT);
	assert_non_null(streams);
	fixture->bis_data = malloc(CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE);
	assert_non_null(fixture->bis_data);

	/* Memset everything to 0 */
	memset(fixture->param, 0, sizeof(*fixture->param));
	memset(subgroup_param, 0,
	       sizeof(struct bt_bap_broadcast_source_subgroup_param) *
		       CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT);
	memset(stream_params, 0,
	       sizeof(struct bt_bap_broadcast_source_stream_param) *
		       CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT);
	memset(fixture->codec_cfg, 0, sizeof(struct bt_audio_codec_cfg));
	memset(fixture->qos_cfg, 0, sizeof(struct bt_bap_qos_cfg));
	memset(streams, 0, sizeof(struct bt_bap_stream) * CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT);
	memset(fixture->bis_data, 0, CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE);

	/* Initialize default values*/
	*fixture->codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10, loc, 40U, 1, ctx);
	*fixture->qos_cfg = BT_BAP_QOS_CFG_UNFRAMED(10000u, sdu, rtn, latency, pd);
	memcpy(fixture->bis_data, bis_cfg_data, sizeof(bis_cfg_data));

	for (size_t i = 0U; i < CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT; i++) {
		subgroup_param[i].params_count = streams_per_subgroup;
		subgroup_param[i].params = stream_params + i * streams_per_subgroup;
		subgroup_param[i].codec_cfg = fixture->codec_cfg;
	}

	for (size_t i = 0U; i < CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT; i++) {
		stream_params[i].stream = &streams[i];
		stream_params[i].data = fixture->bis_data;
		stream_params[i].data_len = sizeof(bis_cfg_data);
		bt_bap_stream_cb_register(stream_params[i].stream, &mock_bap_stream_ops);
	}

	fixture->param->params_count = CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT;
	fixture->param->params = subgroup_param;
	fixture->param->qos = fixture->qos_cfg;
	fixture->param->encryption = false;
	memset(fixture->param->broadcast_code, 0, sizeof(fixture->param->broadcast_code));
	fixture->param->packing = BT_ISO_PACKING_SEQUENTIAL;

	fixture->stream_cnt = fixture->param->params_count * streams_per_subgroup;
}

static void *bap_broadcast_source_test_suite_setup(void)
{
	struct bap_broadcast_source_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void bap_broadcast_source_test_suite_before(void *f)
{
	int err;

	memset(f, 0, sizeof(struct bap_broadcast_source_test_suite_fixture));
	bap_broadcast_source_test_suite_fixture_init(f);

	err = bt_bap_broadcast_source_register_cb(&mock_bap_broadcast_source_cb);
	assert_int_equal(0, err);
}

static void bap_broadcast_source_test_suite_after(void *f)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = f;
	struct bt_bap_broadcast_source_param *param;
	int err;

	if (fixture->source != NULL) {
		err = bt_bap_broadcast_source_stop(fixture->source);
		assert_true(err == 0 || err == -EBADMSG || err == -EALREADY);

		err = bt_bap_broadcast_source_delete(fixture->source);
		assert_int_equal(0, err);
		fixture->source = NULL;
	}

	param = fixture->param;

	free(param->params[0].params[0].stream);
	free(param->params[0].params);
	free(fixture->codec_cfg);
	free(fixture->bis_data);
	free(fixture->qos_cfg);
	free(param->params);
	free(param);

	err = bt_bap_broadcast_source_unregister_cb(&mock_bap_broadcast_source_cb);
	assert_true(err == 0 || err == -ENOENT);
}

static void bap_broadcast_source_test_suite_teardown(void *f)
{
	free(f);
}



static void bap_broadcast_source_test_suite__test_broadcast_source_create_delete(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	for (size_t i = 0u; i < create_param->params_count; i++) {
		for (size_t j = 0u; j < create_param->params[i].params_count; j++) {
			const struct bt_bap_stream *stream =
				create_param->params[i].params[j].stream;

			assert_int_equal(create_param->qos->sdu, stream->qos->sdu);
			assert_int_equal(create_param->qos->rtn, stream->qos->rtn);
			assert_int_equal(create_param->qos->phy, stream->qos->phy);
		}
	}

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_start_send_stop_delete(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_le_ext_adv ext_adv = {0};
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(fixture->source, &ext_adv);
	assert_int_equal(0, err);

	zexpect_call_count("bt_bap_stream_ops.connected", fixture->stream_cnt,
			   mock_bap_stream_connected_cb_fake.call_count);
	zexpect_call_count("bt_bap_stream_ops.started", fixture->stream_cnt,
			   mock_bap_stream_started_cb_fake.call_count);
	zexpect_call_count("bt_bap_broadcast_source_cb.started", 1,
			   mock_bap_broadcast_source_started_cb_fake.call_count);

	for (size_t i = 0U; i < create_param->params_count; i++) {
		for (size_t j = 0U; j < create_param->params[i].params_count; j++) {
			struct bt_bap_stream *bap_stream = create_param->params[i].params[j].stream;

			/* verify bap stream started cb stream parameter */
			assert_int_equal(mock_bap_stream_started_cb_fake.arg0_history[i], bap_stream);
			const struct bt_audio_codec_cfg *codec_cfg = bap_stream->codec_cfg;
			enum bt_audio_location chan_allocation;
			/* verify subgroup codec data */
			assert_int_equal(bt_audio_codec_cfg_get_freq(codec_cfg), BT_AUDIO_CODEC_CFG_FREQ_16KHZ);
			assert_int_equal(bt_audio_codec_cfg_get_frame_dur(codec_cfg), BT_AUDIO_CODEC_CFG_DURATION_10);
			/* verify bis specific codec data */
			assert_int_equal(bt_audio_codec_cfg_get_chan_allocation(codec_cfg,
									  &chan_allocation, false), 0);
			assert_int_equal(chan_allocation, BT_AUDIO_LOCATION_FRONT_LEFT | BT_AUDIO_LOCATION_FRONT_RIGHT);
			/* Since BAP doesn't care about the `buf` we can just provide NULL */
			err = bt_bap_stream_send(bap_stream, NULL, 0);
			assert_int_equal(0, err);
		}
	}

	zexpect_call_count("bt_bap_stream_ops.sent", fixture->stream_cnt,
			   mock_bap_stream_sent_cb_fake.call_count);

	err = bt_bap_broadcast_source_stop(fixture->source);
	assert_int_equal(0, err);

	zexpect_call_count("bt_bap_stream_ops.disconnected", fixture->stream_cnt,
			   mock_bap_stream_disconnected_cb_fake.call_count);
	zexpect_call_count("bt_bap_stream_ops.stopped", fixture->stream_cnt,
			   mock_bap_stream_stopped_cb_fake.call_count);
	zexpect_call_count("bt_bap_broadcast_source_cb.stopped", 1,
			   mock_bap_broadcast_source_stopped_cb_fake.call_count);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_param_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_bap_broadcast_source_create(NULL, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_source_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	err = bt_bap_broadcast_source_create(create_param, NULL);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_count_0(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	create_param->params_count = 0U;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_count_above_max(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	create_param->params_count = CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT + 1;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	int err;

	create_param->params = NULL;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	/* Restore the params for the cleanup after function */
	create_param->params = subgroup_params;
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_qos_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	const struct bt_bap_qos_cfg *qos = create_param->qos;
	int err;

	create_param->qos = NULL;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	/* Restore the params for the cleanup after function */
	create_param->qos = qos;
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_packing(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	create_param->packing = 0x02U;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_params_count_0(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	int err;

	subgroup_params->params_count = 0U;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_params_count_above_max(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	int err;

	subgroup_params->params_count = CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT + 1;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_stream_params_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	int err;

	subgroup_params->params = NULL;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	/* Restore the params for the cleanup after function */
	subgroup_params->params = stream_params;
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	const struct bt_audio_codec_cfg *codec_cfg = subgroup_params->codec_cfg;
	int err;

	subgroup_params->codec_cfg = NULL;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	/* Restore the params for the cleanup after function */
	subgroup_params->codec_cfg = codec_cfg;
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_data_len(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	codec_cfg->data_len = CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE + 1;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_meta_len(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	codec_cfg->meta_len = CONFIG_BT_AUDIO_CODEC_CFG_MAX_METADATA_SIZE + 1;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_cid(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	codec_cfg->id = BT_HCI_CODING_FORMAT_LC3;
	codec_cfg->cid = 0x01U; /* Shall be 0 if id == 0x06 (LC3)*/
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_vid(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	codec_cfg->id = BT_HCI_CODING_FORMAT_LC3;
	codec_cfg->vid = 0x01U; /* Shall be 0 if id == 0x06 (LC3)*/
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_stream_params_stream_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	struct bt_bap_stream *stream = stream_params->stream;
	int err;

	stream_params->stream = NULL;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	/* Restore the params for the cleanup after function */
	stream_params->stream = stream;
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_stream_params_data_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	const uint8_t *data = stream_params->data;
	int err;

	stream_params->data = NULL;
	stream_params->data_len = 1U;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	/* Restore the params for the cleanup after function */
	stream_params->data = data;
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_create_inval_stream_params_data_len(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &create_param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	int err;

	stream_params->data_len = CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE + 1;
	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_start_inval_source_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_le_ext_adv ext_adv = {0};
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(NULL, &ext_adv);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_start_inval_ext_adv_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(fixture->source, NULL);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_start_inval_double_start(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_le_ext_adv ext_adv = {0};
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(fixture->source, &ext_adv);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(fixture->source, &ext_adv);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_single_subgroup(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *reconf_param = fixture->param;
	const size_t subgroup_cnt = reconf_param->params_count;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       reconf_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(reconf_param, &fixture->source);
	assert_int_equal(0, err);

	for (size_t i = 0u; i < reconf_param->params_count; i++) {
		for (size_t j = 0u; j < reconf_param->params[i].params_count; j++) {
			const struct bt_bap_stream *stream =
				reconf_param->params[i].params[j].stream;

			assert_int_equal(reconf_param->qos->sdu, stream->qos->sdu);
			assert_int_equal(reconf_param->qos->rtn, stream->qos->rtn);
			assert_int_equal(reconf_param->qos->phy, stream->qos->phy);
		}
	}

	reconf_param->params_count = 1U;
	fixture->qos_cfg->sdu = 100U;
	fixture->qos_cfg->rtn = 3U;
	fixture->qos_cfg->phy = 1U;

	err = bt_bap_broadcast_source_reconfig(fixture->source, reconf_param);
	assert_int_equal(0, err);

	for (size_t i = 0u; i < subgroup_cnt; i++) {
		for (size_t j = 0u; j < reconf_param->params[i].params_count; j++) {
			const struct bt_bap_stream *stream =
				reconf_param->params[i].params[j].stream;

			assert_int_equal(reconf_param->qos->sdu, stream->qos->sdu);
			assert_int_equal(reconf_param->qos->rtn, stream->qos->rtn);
			assert_int_equal(reconf_param->qos->phy, stream->qos->phy);
		}
	}

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_all(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *reconf_param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       reconf_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(reconf_param, &fixture->source);
	assert_int_equal(0, err);

	for (size_t i = 0u; i < reconf_param->params_count; i++) {
		for (size_t j = 0u; j < reconf_param->params[i].params_count; j++) {
			const struct bt_bap_stream *stream =
				reconf_param->params[i].params[j].stream;

			assert_int_equal(reconf_param->qos->sdu, stream->qos->sdu);
			assert_int_equal(reconf_param->qos->rtn, stream->qos->rtn);
			assert_int_equal(reconf_param->qos->phy, stream->qos->phy);
		}
	}

	fixture->qos_cfg->sdu = 100U;
	fixture->qos_cfg->rtn = 3U;
	fixture->qos_cfg->phy = 1U;

	err = bt_bap_broadcast_source_reconfig(fixture->source, reconf_param);
	assert_int_equal(0, err);

	for (size_t i = 0u; i < reconf_param->params_count; i++) {
		for (size_t j = 0u; j < reconf_param->params[i].params_count; j++) {
			const struct bt_bap_stream *stream =
				reconf_param->params[i].params[j].stream;

			assert_int_equal(reconf_param->qos->sdu, stream->qos->sdu);
			assert_int_equal(reconf_param->qos->rtn, stream->qos->rtn);
			assert_int_equal(reconf_param->qos->phy, stream->qos->phy);
		}
	}

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_param_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_reconfig(fixture->source, NULL);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_source_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_reconfig(NULL, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_count_0(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	param->params_count = 0U;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_count_above_max(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	param->params_count = CONFIG_BT_BAP_BROADCAST_SRC_SUBGROUP_COUNT + 1;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	param->params = NULL;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	/* Restore the params for the cleanup after function */
	param->params = subgroup_params;
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_qos_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	param->qos = NULL;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_packing(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	param->packing = 0x02U;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_params_count_0(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	subgroup_params->params_count = 0U;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_params_count_above_max(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	subgroup_params->params_count = CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT + 1;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_stream_params_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	subgroup_params->params = NULL;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	/* Restore the params for the cleanup after function */
	subgroup_params->params = stream_params;
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	const struct bt_audio_codec_cfg *codec_cfg = subgroup_params->codec_cfg;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	subgroup_params->codec_cfg = NULL;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	/* Restore the params for the cleanup after function */
	subgroup_params->codec_cfg = codec_cfg;
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_data_len(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	codec_cfg->data_len = CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE + 1;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_meta_len(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	codec_cfg->meta_len = CONFIG_BT_AUDIO_CODEC_CFG_MAX_METADATA_SIZE + 1;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_cid(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	codec_cfg->id = 0x06U;
	codec_cfg->cid = 0x01U; /* Shall be 0 if id == 0x06 (LC3)*/
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_vid(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_audio_codec_cfg *codec_cfg = fixture->codec_cfg;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	codec_cfg->id = 0x06U;
	codec_cfg->vid = 0x01U; /* Shall be 0 if id == 0x06 (LC3)*/
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_stream_params_stream_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	struct bt_bap_stream *stream = stream_params->stream;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	stream_params->stream = NULL;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	/* Restore the params for the cleanup after function */
	stream_params->stream = stream;
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_stream_params_data_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	const uint8_t *data = stream_params->data;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	stream_params->data = NULL;
	stream_params->data_len = 1U;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	/* Restore the params for the cleanup after function */
	stream_params->data = data;
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_stream_params_data_len(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source_subgroup_param *subgroup_params = &param->params[0];
	struct bt_bap_broadcast_source_stream_param *stream_params = &subgroup_params->params[0];
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);

	stream_params->data_len = CONFIG_BT_AUDIO_CODEC_CFG_MAX_DATA_SIZE + 1;
	err = bt_bap_broadcast_source_reconfig(fixture->source, param);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_state(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *param = fixture->param;
	struct bt_bap_broadcast_source *source;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(param, &fixture->source);
	assert_int_equal(0, err);
	source = fixture->source;

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;

	err = bt_bap_broadcast_source_reconfig(source, param);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_stop_inval_source_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_le_ext_adv ext_adv = {0};
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(fixture->source, &ext_adv);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_stop(NULL);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_stop_inval_state(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_le_ext_adv ext_adv = {0};
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_start(fixture->source, &ext_adv);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_stop(fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_stop(NULL);
	assert_int_not_equal(0, err);
}


static void bap_broadcast_source_test_suite__test_broadcast_source_delete_inval_source_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_bap_broadcast_source_delete(NULL);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_delete_inval_double_start(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source *source;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);

	source = fixture->source;
	/* Set to NULL to avoid deleting it in bap_broadcast_source_test_suite_after */
	fixture->source = NULL;

	err = bt_bap_broadcast_source_delete(source);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_get_base_single_bis(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	const uint8_t expected_base[] = {
		0x51U, 0x18U,                              /* uuid */
		0x40U, 0x9CU, 0x00U,                       /* pd */
		0x01U,                                     /* subgroup count */
		0x01U,                                     /* bis count */
		0x06U, 0x00U, 0x00U, 0x00U, 0x00U,         /* LC3 codec_id*/
		0x10U,                                     /* cc length */
		0x02U, 0x01U, 0x03U, 0x02U,
		0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U,
		0x03U, 0x04U, 0x28U, 0x00U,                /* cc */
		0x04U,                                     /* meta length */
		0x03U, 0x02U, 0x01U, 0x00U,                /* meta */
		0x01U,                                     /* bis index */
		0x06U,                                     /* bis cc length */
		0x05U, 0x03U, 0x03U, 0x00U, 0x00U, 0x00U   /* bis cc length */
	};

	NET_BUF_SIMPLE_DEFINE(base_buf, 64);

	/* Make the create param simpler for verification */
	create_param->params_count = 1U;
	create_param->params[0].params_count = 1U;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_get_base(fixture->source, &base_buf);
	assert_int_equal(0, err);

	assert_int_equal(sizeof(expected_base), base_buf.len);

	/* Use memcmp to print the buffers if they are not identical as zassert_mem_equal does not
	 * do that
	 */
	if (memcmp(expected_base, base_buf.data, base_buf.len) != 0) {
		for (size_t i = 0U; i < base_buf.len; i++) {
			bt_log_print("[%zu]: 0x%02X %s 0x%02X\n", i, expected_base[i],
			       expected_base[i] == base_buf.data[i] ? "==" : "!=",
			       base_buf.data[i]);
		}

		assert_memory_equal(expected_base, base_buf.data, base_buf.len);
	}

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_get_base(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	const uint8_t expected_base[] = {
		0x51U, 0x18U,                              /* uuid */
		0x40U, 0x9CU, 0x00U,                       /* pd */
		0x02U,                                     /* subgroup count */
		0x01U,                                     /* Subgroup 1: bis count */
		0x06U, 0x00U, 0x00U, 0x00U, 0x00U,         /* LC3 codec_id*/
		0x10U,                                     /* cc length */
		0x02U, 0x01U, 0x03U, 0x02U,
		0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U,
		0x03U, 0x04U, 0x28U, 0x00U,                /* cc */
		0x04U,                                     /* meta length */
		0x03U, 0x02U, 0x01U, 0x00U,                /* meta */
		0x01U,                                     /* bis index */
		0x06U,                                     /* bis cc length */
		0x05U, 0x03U, 0x03U, 0x00U, 0x00U, 0x00U,  /* bis cc length */
		0x01U,                                     /* Subgroup 1: bis count */
		0x06U, 0x00U, 0x00U, 0x00U, 0x00U,         /* LC3 codec_id*/
		0x10U,                                     /* cc length */
		0x02U, 0x01U, 0x03U, 0x02U,
		0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U,
		0x03U, 0x04U, 0x28U, 0x00U,                /* cc */
		0x04U,                                     /* meta length */
		0x03U, 0x02U, 0x01U, 0x00U,                /* meta */
		0x02U,                                     /* bis index */
		0x06U,                                     /* bis cc length */
		0x05U, 0x03U, 0x03U, 0x00U, 0x00U, 0x00U   /* bis cc length */
	};

	NET_BUF_SIMPLE_DEFINE(base_buf, 128);

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_get_base(fixture->source, &base_buf);
	assert_int_equal(0, err);

	assert_int_equal(sizeof(expected_base), base_buf.len);

	/* Use memcmp to print the buffers if they are not identical as zassert_mem_equal does not
	 * do that
	 */
	if (memcmp(expected_base, base_buf.data, base_buf.len) != 0) {
		for (size_t i = 0U; i < base_buf.len; i++) {
			bt_log_print("[%zu]: 0x%02X %s 0x%02X\n", i, expected_base[i],
			       expected_base[i] == base_buf.data[i] ? "==" : "!=",
			       base_buf.data[i]);
		}

		assert_memory_equal(expected_base, base_buf.data, base_buf.len);
	}

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_source_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	int err;

	NET_BUF_SIMPLE_DEFINE(base_buf, 64);

	err = bt_bap_broadcast_source_get_base(NULL, &base_buf);
	assert_int_not_equal(0, err);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_base_buf_null(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_get_base(fixture->source, NULL);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_state(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	struct bt_bap_broadcast_source *source;
	int err;

	NET_BUF_SIMPLE_DEFINE(base_buf, 64);

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	source = fixture->source;

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;

	err = bt_bap_broadcast_source_get_base(source, &base_buf);
	assert_int_not_equal(0, err);
}

/** This tests that providing a buffer too small for _any_ BASE fails correctly */
static void bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_very_small_buf(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	NET_BUF_SIMPLE_DEFINE(base_buf, 15); /* Too small to hold any BASE */

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_get_base(fixture->source, &base_buf);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

/** This tests that providing a buffer too small for the BASE we want to setup fails correctly */
static void bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_small_buf(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_broadcast_source_param *create_param = fixture->param;
	int err;

	/* Can hold a base, but not large enough for this configuration */
	NET_BUF_SIMPLE_DEFINE(base_buf, 64);

	bt_log_print("Creating broadcast source with %zu subgroups with %zu streams\n",
	       create_param->params_count, fixture->stream_cnt);

	err = bt_bap_broadcast_source_create(create_param, &fixture->source);
	assert_int_equal(0, err);

	err = bt_bap_broadcast_source_get_base(fixture->source, &base_buf);
	assert_int_not_equal(0, err);

	err = bt_bap_broadcast_source_delete(fixture->source);
	assert_int_equal(0, err);
	fixture->source = NULL;
}

static bool bap_broadcast_source_foreach_stream_cb(struct bt_bap_stream *stream, void *user_data)
{
	size_t *cnt = user_data;

	ARG_UNUSED(stream);

	(*cnt)++;

	return true;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	size_t cnt = 0U;
	int err;

	err = bt_bap_broadcast_source_create(fixture->param, &fixture->source);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_foreach_stream(fixture->source,
						     bap_broadcast_source_foreach_stream_cb, &cnt);
	assert_int_equal(err, 0);
	assert_int_equal(cnt, CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT);
}

static bool bap_broadcast_source_foreach_stream_return_early_cb(struct bt_bap_stream *stream,
								void *user_data)
{
	size_t *cnt = user_data;

	ARG_UNUSED(stream);

	(*cnt)++;

	return false;
}

static void bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream_return_early(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	size_t cnt = 0U;
	int err;

	err = bt_bap_broadcast_source_create(fixture->param, &fixture->source);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_foreach_stream(
		fixture->source, bap_broadcast_source_foreach_stream_return_early_cb, &cnt);
	assert_int_equal(err, -ECANCELED);
	assert_int_equal(cnt, 1U);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream_inval_null_source(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	size_t cnt = 0U;
	int err;

	err = bt_bap_broadcast_source_create(fixture->param, &fixture->source);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_foreach_stream(NULL, bap_broadcast_source_foreach_stream_cb,
						     &cnt);
	assert_int_equal(err, -EINVAL);
	assert_int_equal(cnt, 0U);
}

static void bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream_inval_null_func(void **cm_state)
{
	struct bap_broadcast_source_test_suite_fixture *fixture = *cm_state;
	size_t cnt = 0U;
	int err;

	err = bt_bap_broadcast_source_create(fixture->param, &fixture->source);
	assert_int_equal(err, 0);

	err = bt_bap_broadcast_source_foreach_stream(fixture->source, NULL, &cnt);
	assert_int_equal(err, -EINVAL);
	assert_int_equal(cnt, 0U);
}

int bt_test_bap_broadcast_source_test_suite_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = bap_broadcast_source_test_suite_setup();
	return 0;
}
static int bt_test_bap_broadcast_source_test_suite_before(void **state)
{
	(void)state;
	bap_broadcast_source_test_suite_before(*state);
	mock_init_rule_before_a2543a(NULL, *state);
	mock_init_rule_before_17cc08(NULL, *state);
	return 0;
}
int bt_test_bap_broadcast_source_test_suite_after(void **state)
{
	(void)state;
	bap_broadcast_source_test_suite_after(*state);
	mock_destroy_rule_after_17cc08(NULL, *state);
	return 0;
}
int bt_test_bap_broadcast_source_test_suite_teardown(void **state)
{
	(void)state;
	bap_broadcast_source_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_bap_broadcast_source_test_suite[] = {
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_delete, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_start_send_stop_delete, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_param_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_source_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_count_0, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_count_above_max, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_qos_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_packing, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_params_count_0, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_params_count_above_max, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_stream_params_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_data_len, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_meta_len, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_cid, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_subgroup_params_codec_cfg_vid, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_stream_params_stream_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_stream_params_data_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_create_inval_stream_params_data_len, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_start_inval_source_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_start_inval_ext_adv_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_start_inval_double_start, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_single_subgroup, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_all, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_param_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_source_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_count_0, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_count_above_max, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_qos_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_packing, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_params_count_0, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_params_count_above_max, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_stream_params_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_data_len, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_meta_len, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_cid, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_subgroup_params_codec_cfg_vid, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_stream_params_stream_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_stream_params_data_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_stream_params_data_len, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_reconfigure_inval_state, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_stop_inval_source_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_stop_inval_state, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_delete_inval_source_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_delete_inval_double_start, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base_single_bis, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_source_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_base_buf_null, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_state, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_very_small_buf, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_get_base_inval_small_buf, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream_return_early, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream_inval_null_source, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_broadcast_source_test_suite__test_broadcast_source_foreach_stream_inval_null_func, bt_test_bap_broadcast_source_test_suite_before, bt_test_bap_broadcast_source_test_suite_after),
};
BT_TEST_SUITE(bap_broadcast_source_test_suite, test_cases_bap_broadcast_source_test_suite, bt_test_bap_broadcast_source_test_suite_setup, bt_test_bap_broadcast_source_test_suite_teardown);
