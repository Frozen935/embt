#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_26f0f3(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_26f0f3(const struct bt_test_case_info *, void *);


/* test_ase_state_transition.c - ASE state transition tests */

/*
 * Copyright (c) 2023 Codecoup
 * Copyright (c) 2024 Demant A/S
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <embt/bluetooth/assigned_numbers.h>
#include <embt/bluetooth/audio/ascs.h>
#include <embt/bluetooth/audio/lc3.h>
#include <embt/bluetooth/byteorder.h>
#include <embt/bluetooth/gap.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/iso.h>

#include <embt/base/util.h>
#include <embt/compiler.h>
#include <stddef.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/gatt.h>
#include <embt/base/util_macro.h>
#include "audio/ascs_internal.h"

#include "ascs.h"
#include "ascs_expects.h"
#include "bap_stream.h"
#include "bap_stream_expects.h"
#include "conn.h"
#include "iso.h"

#include "test_common.h"

#define test_sink_ase_state_transition_fixture test_ase_state_transition_fixture
#define test_source_ase_state_transition_fixture test_ase_state_transition_fixture

static const struct bt_bap_qos_cfg_pref qos_pref =
	BT_BAP_QOS_CFG_PREF(true, BT_GAP_LE_PHY_2M, 0x02U, 10U, 40000U, 40000U, 40000U, 40000U);

struct test_ase_state_transition_fixture {
	struct bt_conn conn;
	struct bt_bap_stream stream;
	struct {
		uint8_t id;
		const struct bt_gatt_attr *attr;
	} ase;
};

static void *test_sink_ase_state_transition_setup(void)
{
	struct test_ase_state_transition_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void test_ase_snk_state_transition_before(void *f)
{
	struct test_ase_state_transition_fixture *fixture =
		(struct test_ase_state_transition_fixture *) f;
	struct bt_ascs_register_param param = {
		.snk_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT,
		.src_cnt = CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT,
		.cb = &mock_ascs_cb,
	};
	int err;

	err = bt_ascs_register(&param);
	assert_int_equal(err, 0);

	memset(fixture, 0, sizeof(struct test_ase_state_transition_fixture));
	test_conn_init(&fixture->conn);
	test_ase_snk_get(1, &fixture->ase.attr);
	if (fixture->ase.attr != NULL) {
		fixture->ase.id = test_ase_id_get(fixture->ase.attr);
	}

	bt_bap_stream_cb_register(&fixture->stream, &mock_bap_stream_ops);
}

static void test_ase_src_state_transition_before(void *f)
{
	struct test_ase_state_transition_fixture *fixture =
		(struct test_ase_state_transition_fixture *) f;
	struct bt_ascs_register_param param = {
		.snk_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT,
		.src_cnt = CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT,
		.cb = &mock_ascs_cb,
	};
	int err;

	err = bt_ascs_register(&param);
	assert_int_equal(err, 0);

	memset(fixture, 0, sizeof(struct test_ase_state_transition_fixture));
	test_conn_init(&fixture->conn);
	test_ase_src_get(CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT, &fixture->ase.attr);
	if (fixture->ase.attr != NULL) {
		fixture->ase.id = test_ase_id_get(fixture->ase.attr);
	}

	bt_bap_stream_cb_register(&fixture->stream, &mock_bap_stream_ops);
}

static void test_ase_state_transition_after(void *f)
{
	struct test_ase_state_transition_fixture *fixture =
		(struct test_ase_state_transition_fixture *)f;
	int err;

	if (fixture->conn.info.state == BT_CONN_STATE_CONNECTED) {
		mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_LOCALHOST_TERM_CONN);
	}

	err = bt_ascs_unregister();
	assert_int_equal(err, 0);
}

static void test_ase_state_transition_teardown(void *f)
{
	free(f);
}



static void test_sink_ase_state_transition__test_client_idle_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Verification */
	enum bt_audio_dir dir = BT_AUDIO_DIR_SINK;

	expect_bt_ascs_cb_config_called(1, &conn, NULL, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_sink_ase_state_transition__test_client_codec_configured_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	test_ase_control_client_config_qos(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_qos_called(1, &stream, NULL);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_client_qos_configured_to_enabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_enable(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_enable_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_enabled_called(1, &stream);
}

static void test_sink_ase_state_transition__test_client_enabling_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	test_ase_control_client_disable(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_sink_ase_state_transition__test_client_qos_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_release(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_sink_ase_state_transition__test_client_codec_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SINK;

	expect_bt_ascs_cb_config_called(0, NULL, NULL, NULL, NULL);
	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_sink_ase_state_transition__test_client_qos_configured_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_config_qos(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_qos_called(1, &stream, NULL);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_client_qos_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SINK;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_sink_ase_state_transition__test_client_codec_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	test_ase_control_client_release(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_sink_ase_state_transition__test_client_enabling_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	test_ase_control_client_release(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_client_enabling_to_enabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	test_ase_control_client_update_metadata(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_client_streaming_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	test_ase_control_client_release(conn, ase_id);

	/* Client disconnects the ISO */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;

	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_sink_ase_state_transition__test_client_streaming_to_streaming(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	test_ase_control_client_update_metadata(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_client_streaming_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	test_ase_control_client_disable(conn, ase_id);

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;

	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_sink_ase_state_transition__test_server_idle_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_audio_codec_cfg codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT, 40U, 1, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	err = bt_ascs_config_ase(conn, stream, &codec_cfg, &qos_pref);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_config_called(0, NULL, NULL, NULL, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_sink_ase_state_transition__test_server_codec_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_audio_codec_cfg codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT, 40U, 1, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	err = bt_ascs_reconfig_ase(stream->ep, &codec_cfg);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SINK;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_sink_ase_state_transition__test_server_codec_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_sink_ase_state_transition__test_server_qos_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_audio_codec_cfg codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT, 40U, 1, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	err = bt_ascs_reconfig_ase(stream->ep, &codec_cfg);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SINK;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_sink_ase_state_transition__test_server_qos_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_sink_ase_state_transition__test_server_enabling_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_server_enabling_to_enabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	const uint8_t meta[] = {
		BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT,
				    BT_BYTES_LIST_LE16(BT_AUDIO_CONTEXT_TYPE_MEDIA)),
	};
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = bt_ascs_metadata_ase(stream->ep, meta, ARRAY_SIZE(meta));
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_server_enabling_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = bt_ascs_disable_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_sink_ase_state_transition__test_server_enabling_to_streaming(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	err = bt_ascs_start_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_bap_stream_ops_connected_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_started_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
	/* XXX: ascs_cb->start is not called for Sink ASE */
}

static void test_sink_ase_state_transition__test_server_streaming_to_streaming(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	const uint8_t meta[] = {
		BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT,
				    BT_BYTES_LIST_LE16(BT_AUDIO_CONTEXT_TYPE_MEDIA)),
	};
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	err = bt_ascs_metadata_ase(stream->ep, meta, ARRAY_SIZE(meta));
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_sink_ase_state_transition__test_server_streaming_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	err = bt_ascs_disable_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_LOCALHOST_TERM_CONN;

	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_sink_ase_state_transition__test_server_streaming_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Client disconnects the ISO */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_LOCALHOST_TERM_CONN;

	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);
}

static void *test_source_ase_state_transition_setup(void)
{
	struct test_ase_state_transition_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}



static void test_source_ase_state_transition__test_client_idle_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Verification */
	enum bt_audio_dir dir = BT_AUDIO_DIR_SOURCE;

	expect_bt_ascs_cb_config_called(1, &conn, NULL, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_source_ase_state_transition__test_client_codec_configured_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	test_ase_control_client_config_qos(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_qos_called(1, &stream, NULL);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_qos_configured_to_enabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_enable(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_enable_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_enabled_called(1, &stream);
}

static void test_source_ase_state_transition__test_client_enabling_to_disabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	test_ase_control_client_disable(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_source_ase_state_transition__test_client_qos_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_release(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_source_ase_state_transition__test_client_enabling_to_streaming(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	test_ase_control_client_receiver_start_ready(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_start_called(1, &stream);
	expect_bt_bap_stream_ops_connected_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_started_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_codec_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SOURCE;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_source_ase_state_transition__test_client_qos_configured_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_config_qos(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_qos_called(1, &stream, NULL);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_qos_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SOURCE;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_source_ase_state_transition__test_client_codec_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	test_ase_control_client_release(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_source_ase_state_transition__test_client_enabling_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	test_ase_control_client_release(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_enabling_to_enabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	test_ase_control_client_update_metadata(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_streaming_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);

	test_ase_control_client_release(conn, ase_id);

	/* Client disconnects the ISO */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;

	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_source_ase_state_transition__test_client_streaming_to_streaming(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);

	test_ase_control_client_update_metadata(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_streaming_to_disabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);

	test_ase_control_client_disable(conn, ase_id);

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;

	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_source_ase_state_transition__test_client_enabling_to_disabling_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);
	test_ase_control_client_disable(conn, ase_id);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);

	test_mocks_reset();

	test_ase_control_client_receiver_stop_ready(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_stop_called(1, &stream);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_client_streaming_to_disabling_to_qos_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);
	test_ase_control_client_disable(conn, ase_id);

	/* Verify that stopped callback was called by disable */
	const uint8_t reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;

	expect_bt_bap_stream_ops_disabled_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);


	test_mocks_reset();

	test_ase_control_client_receiver_stop_ready(conn, ase_id);

	/* Verification */
	expect_bt_ascs_cb_stop_called(1, &stream);
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_server_idle_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_audio_codec_cfg codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT, 40U, 1, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	err = bt_ascs_config_ase(conn, stream, &codec_cfg, &qos_pref);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_config_called(0, NULL, NULL, NULL, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_source_ase_state_transition__test_server_codec_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_audio_codec_cfg codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT, 40U, 1, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	err = bt_ascs_reconfig_ase(stream->ep, &codec_cfg);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SOURCE;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_source_ase_state_transition__test_server_codec_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_codec_configured(conn, ase_id, stream);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_source_ase_state_transition__test_server_qos_configured_to_codec_configured(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_audio_codec_cfg codec_cfg = BT_AUDIO_CODEC_LC3_CONFIG(
		BT_AUDIO_CODEC_CFG_FREQ_16KHZ, BT_AUDIO_CODEC_CFG_DURATION_10,
		BT_AUDIO_LOCATION_FRONT_LEFT, 40U, 1, BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	err = bt_ascs_reconfig_ase(stream->ep, &codec_cfg);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const enum bt_audio_dir dir = BT_AUDIO_DIR_SOURCE;

	expect_bt_ascs_cb_reconfig_called(1, &stream, &dir, NULL);
	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

static void test_source_ase_state_transition__test_server_qos_configured_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_qos_configured(conn, ase_id, stream);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_source_ase_state_transition__test_server_enabling_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_server_enabling_to_enabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	const uint8_t meta[] = {
		BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT,
				    BT_BYTES_LIST_LE16(BT_AUDIO_CONTEXT_TYPE_MEDIA)),
	};
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = bt_ascs_metadata_ase(stream->ep, meta, ARRAY_SIZE(meta));
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_server_enabling_to_disabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = bt_ascs_disable_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_source_ase_state_transition__test_server_streaming_to_streaming(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	const uint8_t meta[] = {
		BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT,
				    BT_BYTES_LIST_LE16(BT_AUDIO_CONTEXT_TYPE_MEDIA)),
	};
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);

	err = bt_ascs_metadata_ase(stream->ep, meta, ARRAY_SIZE(meta));
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	expect_bt_ascs_cb_metadata_called(1, &stream, NULL, NULL);
	expect_bt_bap_stream_ops_metadata_updated_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
}

static void test_source_ase_state_transition__test_server_streaming_to_disabling(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);

	err = bt_ascs_disable_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_LOCALHOST_TERM_CONN;

	expect_bt_ascs_cb_disable_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
}

static void test_source_ase_state_transition__test_server_streaming_to_releasing(void **cm_state)
{
	struct test_ase_state_transition_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase.id;
	struct bt_iso_chan *chan;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, true);

	err = bt_ascs_release_ase(stream->ep);
	assert_false(err < 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Client disconnects the ISO */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Verification */
	const uint8_t reason = BT_HCI_ERR_LOCALHOST_TERM_CONN;

	expect_bt_ascs_cb_release_called(1, &stream);
	expect_bt_bap_stream_ops_stopped_called(1, &stream, &reason);
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);
}

int bt_test_test_source_ase_state_transition_setup(void **state)
{
	int environment_result = bt_test_audio_ascs_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = test_source_ase_state_transition_setup();
	return 0;
}
static int bt_test_test_source_ase_state_transition_before(void **state)
{
	(void)state;
	test_ase_src_state_transition_before(*state);
	mock_init_rule_before_26f0f3(NULL, *state);
	return 0;
}
int bt_test_test_source_ase_state_transition_after(void **state)
{
	(void)state;
	test_ase_state_transition_after(*state);
	mock_destroy_rule_after_26f0f3(NULL, *state);
	return 0;
}
int bt_test_test_source_ase_state_transition_teardown(void **state)
{
	(void)state;
	test_ase_state_transition_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_test_source_ase_state_transition[] = {
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_idle_to_codec_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_codec_configured_to_qos_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_qos_configured_to_enabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_enabling_to_disabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_qos_configured_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_enabling_to_streaming, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_codec_configured_to_codec_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_qos_configured_to_qos_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_qos_configured_to_codec_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_codec_configured_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_enabling_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_enabling_to_enabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_streaming_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_streaming_to_streaming, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_streaming_to_disabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_enabling_to_disabling_to_qos_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_client_streaming_to_disabling_to_qos_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_idle_to_codec_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_codec_configured_to_codec_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_codec_configured_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_qos_configured_to_codec_configured, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_qos_configured_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_enabling_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_enabling_to_enabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_enabling_to_disabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_streaming_to_streaming, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_streaming_to_disabling, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_source_ase_state_transition__test_server_streaming_to_releasing, bt_test_test_source_ase_state_transition_before, bt_test_test_source_ase_state_transition_after),
};
BT_TEST_SUITE(test_source_ase_state_transition, test_cases_test_source_ase_state_transition, bt_test_test_source_ase_state_transition_setup, bt_test_test_source_ase_state_transition_teardown);

int bt_test_test_sink_ase_state_transition_setup(void **state)
{
	int environment_result = bt_test_audio_ascs_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = test_sink_ase_state_transition_setup();
	return 0;
}
static int bt_test_test_sink_ase_state_transition_before(void **state)
{
	(void)state;
	test_ase_snk_state_transition_before(*state);
	mock_init_rule_before_26f0f3(NULL, *state);
	return 0;
}
int bt_test_test_sink_ase_state_transition_after(void **state)
{
	(void)state;
	test_ase_state_transition_after(*state);
	mock_destroy_rule_after_26f0f3(NULL, *state);
	return 0;
}
int bt_test_test_sink_ase_state_transition_teardown(void **state)
{
	(void)state;
	test_ase_state_transition_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_test_sink_ase_state_transition[] = {
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_idle_to_codec_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_codec_configured_to_qos_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_qos_configured_to_enabling, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_enabling_to_qos_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_qos_configured_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_codec_configured_to_codec_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_qos_configured_to_qos_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_qos_configured_to_codec_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_codec_configured_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_enabling_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_enabling_to_enabling, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_streaming_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_streaming_to_streaming, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_client_streaming_to_qos_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_idle_to_codec_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_codec_configured_to_codec_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_codec_configured_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_qos_configured_to_codec_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_qos_configured_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_enabling_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_enabling_to_enabling, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_enabling_to_qos_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_enabling_to_streaming, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_streaming_to_streaming, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_streaming_to_qos_configured, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
	cmocka_unit_test_setup_teardown(test_sink_ase_state_transition__test_server_streaming_to_releasing, bt_test_test_sink_ase_state_transition_before, bt_test_test_sink_ase_state_transition_after),
};
BT_TEST_SUITE(test_sink_ase_state_transition, test_cases_test_sink_ase_state_transition, bt_test_test_sink_ase_state_transition_setup, bt_test_test_sink_ase_state_transition_teardown);
