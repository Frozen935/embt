#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_26f0f3(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_26f0f3(const struct bt_test_case_info *, void *);

void mock_init_rule_before_26f0f3(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_26f0f3(const struct bt_test_case_info *, void *);

/* main.c - Application main entry point */

/*
 * Copyright (c) 2023 Codecoup
 * Copyright (c) 2024 Demant A/S
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <embt/bluetooth/audio/ascs.h>
#include <embt/bluetooth/gap.h>
#include <embt/bluetooth/iso.h>
#include <fff.h>

#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/pacs.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/uuid.h>
#include <embt/net_buf.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
#include <embt/compiler.h>
#include <sys/types.h>

#include "assert.h"
#include "ascs.h"
#include "audio/ascs_internal.h"
#include "ascs_expects.h"
#include "bap_stream.h"
#include "bap_stream_expects.h"
#include "conn.h"
#include "gatt.h"
#include "gatt_expects.h"
#include "iso.h"

#include "test_common.h"

DEFINE_FFF_GLOBALS;

void mock_init_rule_before_26f0f3(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	test_mocks_init();
}

void mock_destroy_rule_after_26f0f3(const struct bt_test_case_info *test, void *fixture)
{
	ARG_UNUSED(test);
	ARG_UNUSED(fixture);

	test_mocks_cleanup();
}



struct ascs_test_suite_fixture {
	const struct bt_gatt_attr *ase_cp;
	struct bt_bap_stream stream;
	struct bt_conn conn;
	struct {
		uint8_t id;
		const struct bt_gatt_attr *attr;
	} ase_snk, ase_src;
};

static void ascs_test_suite_fixture_init(struct ascs_test_suite_fixture *fixture)
{
	struct bt_ascs_register_param param = {
		.snk_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT,
		.src_cnt = CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT,
		.cb = &mock_ascs_cb,
	};
	int err;

	memset(fixture, 0, sizeof(*fixture));

	err = bt_ascs_register(&param);
	assert_int_equal(err, 0);

	fixture->ase_cp = test_ase_control_point_get();

	test_conn_init(&fixture->conn);

	test_ase_snk_get(CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT, &fixture->ase_snk.attr);
	if (fixture->ase_snk.attr != NULL) {
		fixture->ase_snk.id = test_ase_id_get(fixture->ase_snk.attr);
	}

	test_ase_src_get(CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT, &fixture->ase_src.attr);
	if (fixture->ase_src.attr != NULL) {
		fixture->ase_src.id = test_ase_id_get(fixture->ase_src.attr);
	}
}

static void *ascs_test_suite_setup(void)
{
	struct ascs_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void ascs_test_suite_before(void *f)
{
	memset(f, 0, sizeof(struct ascs_test_suite_fixture));
	ascs_test_suite_fixture_init(f);
}

static void ascs_test_suite_teardown(void *f)
{
	free(f);
}

static void ascs_test_suite_after(void *f)
{
	struct ascs_test_suite_fixture *fixture = (struct ascs_test_suite_fixture *)f;
	int err;

	if (fixture->conn.info.state == BT_CONN_STATE_CONNECTED) {
		mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_LOCALHOST_TERM_CONN);
	}

	err = bt_ascs_unregister();
	assert_true(err == 0 || err == -EALREADY);
}



static void ascs_test_suite__test_has_sink_ase_chrc(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	assert_non_null(fixture->ase_snk.attr);
}

static void ascs_test_suite__test_has_source_ase_chrc(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	assert_non_null(fixture->ase_src.attr);
}

static void ascs_test_suite__test_has_control_point_chrc(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	assert_non_null(fixture->ase_cp);
}

static void ascs_test_suite__test_sink_ase_read_state_idle(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase = fixture->ase_snk.attr;
	struct bt_conn *conn = &fixture->conn;
	struct test_ase_chrc_value_hdr hdr = { 0xff };
	ssize_t ret;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif

	assert_non_null(fixture->ase_snk.attr);

	ret = ase->read(conn, ase, &hdr, sizeof(hdr), 0);
	assert_false(ret < 0);
	assert_int_equal(0x00, hdr.ase_state);
}

static void ascs_test_suite__test_release_ase_on_unregister(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id;
	int err;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase = fixture->ase_snk.attr;
		ase_id = fixture->ase_snk.id;
	} else {
		ase = fixture->ase_src.attr;
		ase_id = fixture->ase_src.id;
	}

	assert_non_null(ase);
	assert_true(ase_id != 0x00U);

	/* Set ASE to non-idle state */
	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Reset mock, as we expect ASE notification to be sent */
	bt_gatt_notify_cb_reset();

	err = bt_ascs_unregister();
	assert_int_equal(err, 0);

	/* Expected to notify the upper layers */
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);
}

static void ascs_test_suite__test_abort_client_operation_if_callback_not_registered(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	const struct test_ase_cp_chrc_value_param *param;
	const struct test_ase_cp_chrc_value_hdr *hdr;
	const struct bt_gatt_attr *ase_cp = fixture->ase_cp;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	struct bt_gatt_notify_params *notify_params;
	uint8_t ase_id;

	mock_ascs_cb.config = NULL;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase_id = fixture->ase_snk.id;
	} else {
		ase_id = fixture->ase_src.id;
	}

	assert_non_null(ase_cp);
	assert_true(ase_id != 0x00U);

	/* Set ASE to non-idle state */
	test_ase_control_client_config_codec(conn, ase_id, stream);

	/* Expected ASE Control Point notification with Unspecified Error was sent */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, NULL,
					     TEST_ASE_CP_CHRC_VALUE_SIZE(1));

	notify_params = mock_bt_gatt_notify_cb_fake.arg1_val;
	hdr = (void *)notify_params->data;
	assert_int_equal(0x01, hdr->opcode);
	assert_int_equal(0x01, hdr->number_of_ases);
	param = (void *)hdr->params;
	assert_int_equal(ase_id, param->ase_id);
	/* Expect Unspecified Error */
	assert_int_equal(0x0E, param->response_code);
	assert_int_equal(0x00, param->reason);
}

static void ascs_test_suite__test_release_ase_on_acl_disconnection(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	const struct bt_gatt_attr *ase;
	struct bt_iso_chan *chan;
	uint8_t ase_id;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase = fixture->ase_snk.attr;
		ase_id = fixture->ase_snk.id;
	} else {
		ase = fixture->ase_src.attr;
		ase_id = fixture->ase_src.id;
	}

	assert_non_null(ase);
	assert_true(ase_id != 0x00U);

	/* Set ASE to non-idle state */
	test_preamble_state_streaming(conn, ase_id, stream, &chan,
				      !IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK));

	/* Mock ACL disconnection */
	mock_bt_conn_disconnected(conn, BT_HCI_ERR_CONN_TIMEOUT);

	/* Expected to notify the upper layers */
	expect_bt_bap_stream_ops_released_called(1, (const struct bt_bap_stream **)&stream);

	/* Mock CIS disconnection */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_CONN_TIMEOUT);
}

static void ascs_test_suite__test_release_ase_pair_on_acl_disconnection(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase_snk, *ase_src;
	struct bt_bap_stream snk_stream, src_stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_snk_id, ase_src_id;
	struct bt_iso_chan *chan;
	int err;

	if (CONFIG_BT_ASCS_MAX_ACTIVE_ASES < 2) {
		skip();
	}

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif

	memset(&snk_stream, 0, sizeof(snk_stream));
	ase_snk = fixture->ase_snk.attr;
	assert_non_null(ase_snk);
	ase_snk_id = fixture->ase_snk.id;
	assert_true(ase_snk_id != 0x00U);

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif

	memset(&src_stream, 0, sizeof(src_stream));
	ase_src = fixture->ase_src.attr;
	assert_non_null(ase_src);
	ase_src_id = fixture->ase_src.id;
	assert_true(ase_src_id != 0x00U);

	test_ase_control_client_config_codec(conn, ase_snk_id, &snk_stream);
	test_ase_control_client_config_qos(conn, ase_snk_id);
	test_ase_control_client_enable(conn, ase_snk_id);

	test_ase_control_client_config_codec(conn, ase_src_id, &src_stream);
	test_ase_control_client_config_qos(conn, ase_src_id);
	test_ase_control_client_enable(conn, ase_src_id);

	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	test_ase_control_client_receiver_start_ready(conn, ase_src_id);

	err = bt_ascs_start_ase(snk_stream.ep);
	assert_int_equal(0, err);

	test_mocks_reset();

	/* Mock ACL disconnection */
	mock_bt_conn_disconnected(conn, BT_HCI_ERR_CONN_TIMEOUT);

	/* Expected to notify the upper layers */
	const struct bt_bap_stream *streams[] = { &snk_stream, &src_stream };

	expect_bt_bap_stream_ops_released_called(ARRAY_SIZE(streams), streams);

	/* Mock CIS disconnection */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_CONN_TIMEOUT);
}

static void ascs_test_suite__test_recv_in_streaming_state(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase_snk.id;
	struct bt_iso_recv_info info = {
		.seq_num = 1,
		.flags = BT_ISO_FLAGS_VALID,
	};
	struct bt_iso_chan *chan;
	struct net_buf buf;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_streaming(conn, ase_id, stream, &chan, false);

	chan->ops->recv(chan, &info, &buf);

	/* Verification */
	expect_bt_bap_stream_ops_recv_called(1, &stream, &info, &buf);
}

static void ascs_test_suite__test_recv_in_enabling_state(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	uint8_t ase_id = fixture->ase_snk.id;
	struct bt_iso_recv_info info = {
		.seq_num = 1,
		.flags = BT_ISO_FLAGS_VALID,
	};
	struct bt_iso_chan *chan;
	struct net_buf buf;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SNK)
	skip();
#endif


	test_preamble_state_enabling(conn, ase_id, stream);

	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	test_mocks_reset();

	chan->ops->recv(chan, &info, &buf);

	/* Verification */
	expect_bt_bap_stream_ops_recv_called(0, NULL, NULL, NULL);
}

static void ascs_test_suite__test_cis_link_loss_in_streaming_state(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	const struct bt_gatt_attr *ase;
	struct bt_iso_chan *chan;
	uint8_t ase_id;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase = fixture->ase_snk.attr;
		ase_id = fixture->ase_snk.id;
	} else {
		ase = fixture->ase_src.attr;
		ase_id = fixture->ase_src.id;
	}
	assert_non_null(ase);
	assert_true(ase_id != 0x00U);

	test_preamble_state_streaming(conn, ase_id, stream, &chan,
				      !IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK));

	/* Mock CIS disconnection */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_CONN_TIMEOUT);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Expected to notify the upper layers */
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(1, &stream);
	expect_bt_bap_stream_ops_released_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);
}

static void test_cis_link_loss_in_disabling_state(struct ascs_test_suite_fixture *fixture,
						  bool streaming)
{
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	const struct bt_gatt_attr *ase;
	struct bt_iso_chan *chan;
	uint8_t ase_id;
	int err;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	ase = fixture->ase_src.attr;
	ase_id = fixture->ase_src.id;
	assert_non_null(ase);
	assert_true(ase_id != 0x00U);

	test_preamble_state_enabling(conn, ase_id, stream);
	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	if (streaming) {
		test_ase_control_client_receiver_start_ready(conn, ase_id);
	}

	test_ase_control_client_disable(conn, ase_id);

	expect_bt_bap_stream_ops_disabled_called(1, &stream);

	test_mocks_reset();

	/* Mock CIS disconnection */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_CONN_TIMEOUT);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Expected to notify the upper layers */
	expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
	expect_bt_bap_stream_ops_disabled_called(0, NULL);
	expect_bt_bap_stream_ops_released_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);
}

static void ascs_test_suite__test_cis_link_loss_in_disabling_state_v1(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	/* Enabling -> Streaming -> Disabling */
	test_cis_link_loss_in_disabling_state(fixture, true);
}

static void ascs_test_suite__test_cis_link_loss_in_disabling_state_v2(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	/* Enabling -> Disabling */
	test_cis_link_loss_in_disabling_state(fixture, false);
}

static void ascs_test_suite__test_cis_link_loss_in_enabling_state(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	const struct bt_gatt_attr *ase;
	struct bt_iso_chan *chan;
	uint8_t ase_id;
	int err;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase = fixture->ase_snk.attr;
		ase_id = fixture->ase_snk.id;
	} else {
		ase = fixture->ase_src.attr;
		ase_id = fixture->ase_src.id;
	}
	assert_non_null(ase);
	assert_true(ase_id != 0x00U);

	test_preamble_state_enabling(conn, ase_id, stream);
	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	/* Mock CIS disconnection */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_CONN_TIMEOUT);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Expected no change in ASE state */
	expect_bt_bap_stream_ops_qos_configured_called(0, NULL);
	expect_bt_bap_stream_ops_released_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);

	err = bt_ascs_disable_ase(stream->ep);
	assert_int_equal(0, err);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		expect_bt_bap_stream_ops_qos_configured_called(1, &stream);
		expect_bt_bap_stream_ops_disabled_called(1, &stream);
	} else {
		/* Server-initiated disable operation that shall not cause transition to QoS */
		expect_bt_bap_stream_ops_qos_configured_called(0, NULL);
	}
}

static void ascs_test_suite__test_cis_link_loss_in_enabling_state_client_retries(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	const struct bt_gatt_attr *ase;
	struct bt_iso_chan *chan;
	uint8_t ase_id;
	int err;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase = fixture->ase_snk.attr;
		ase_id = fixture->ase_snk.id;
	} else {
		ase = fixture->ase_src.attr;
		ase_id = fixture->ase_src.id;
	}
	assert_non_null(ase);
	assert_true(ase_id != 0x00U);

	test_preamble_state_enabling(conn, ase_id, stream);
	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);
	expect_bt_bap_stream_ops_connected_called(1, (const struct bt_bap_stream **)&stream);

	/* Mock CIS disconnection */
	mock_bt_iso_disconnected(chan, BT_HCI_ERR_CONN_FAIL_TO_ESTAB);

	test_drain_syswq(); /* Ensure that state transitions are completed */

	/* Expected to not notify the upper layers */
	expect_bt_bap_stream_ops_qos_configured_called(0, NULL);
	expect_bt_bap_stream_ops_released_called(0, NULL);
	expect_bt_bap_stream_ops_disconnected_called(1, (const struct bt_bap_stream **)&stream);

	/* Client retries to establish CIS */
	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);
	if (!IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		test_ase_control_client_receiver_start_ready(conn, ase_id);
	} else {
		err = bt_ascs_start_ase(stream->ep);
		assert_int_equal(0, err);
	}

	test_drain_syswq(); /* Ensure that state transitions are completed */

	const struct bt_bap_stream *streams[] = { stream, stream };

	expect_bt_bap_stream_ops_connected_called(ARRAY_SIZE(streams), streams);
	expect_bt_bap_stream_ops_started_called(1, &stream);
}

static struct bt_bap_stream *stream_allocated;
static const struct bt_bap_qos_cfg_pref qos_pref =
	BT_BAP_QOS_CFG_PREF(true, BT_GAP_LE_PHY_2M, 0x02U, 10U, 40000U, 40000U, 40000U, 40000U);

static int ascs_cb_config_custom_fake(struct bt_conn *conn, const struct bt_bap_ep *ep,
				      enum bt_audio_dir dir,
				      const struct bt_audio_codec_cfg *codec_cfg,
				      struct bt_bap_stream **stream,
				      struct bt_bap_qos_cfg_pref *const pref,
				      struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ep);
	ARG_UNUSED(dir);
	ARG_UNUSED(codec_cfg);

	*stream = stream_allocated;
	*pref = qos_pref;
	*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_SUCCESS, BT_BAP_ASCS_REASON_NONE);

	bt_bap_stream_cb_register(*stream, &mock_bap_stream_ops);

	return 0;
}

static void ascs_test_suite__test_ase_state_notification_retry(void **cm_state)
{
	struct ascs_test_suite_fixture *fixture = *cm_state;
	struct bt_bap_stream *stream = &fixture->stream;
	struct bt_conn *conn = &fixture->conn;
	const struct bt_gatt_attr *ase, *cp;
	struct bt_conn_info info;
	uint8_t ase_id;
	int err;

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		ase = fixture->ase_snk.attr;
		ase_id = fixture->ase_snk.id;
	} else {
		ase = fixture->ase_src.attr;
		ase_id = fixture->ase_src.id;
	}

	assert_non_null(ase);
	assert_int_not_equal(ase_id, 0x00U);

	cp = test_ase_control_point_get();
	assert_non_null(cp);

	stream_allocated = stream;
	mock_ascs_cb_config_fake.custom_fake = ascs_cb_config_custom_fake;

	/* Mock out of buffers case */
	mock_bt_gatt_notify_cb_fake.return_val = -ENOMEM;

	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[0] */
	};

	cp->write(conn, cp, (void *)buf, sizeof(buf), 0, 0);

	/* Verification */
	expect_bt_bap_stream_ops_codec_configured_called(0, NULL, NULL);

	mock_bt_gatt_notify_cb_fake.return_val = 0;

	err = bt_conn_get_info(conn, &info);
	assert_int_equal(err, 0);

	/* Wait for ASE state notification retry */
	osi_sleep_ms(BT_MSEC(DIV_ROUND_UP(info.le.interval_us, 1000)));

	expect_bt_bap_stream_ops_codec_configured_called(1, &stream, NULL);
}

int bt_test_ascs_test_suite_setup(void **state)
{
	int environment_result = bt_test_audio_ascs_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = ascs_test_suite_setup();
	return 0;
}
static int bt_test_ascs_test_suite_before(void **state)
{
	(void)state;
	ascs_test_suite_before(*state);
	mock_init_rule_before_26f0f3(NULL, *state);
	return 0;
}
int bt_test_ascs_test_suite_after(void **state)
{
	(void)state;
	ascs_test_suite_after(*state);
	mock_destroy_rule_after_26f0f3(NULL, *state);
	return 0;
}
int bt_test_ascs_test_suite_teardown(void **state)
{
	(void)state;
	ascs_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_ascs_test_suite[] = {
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_has_sink_ase_chrc, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_has_source_ase_chrc, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_has_control_point_chrc, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_sink_ase_read_state_idle, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_release_ase_on_unregister, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_abort_client_operation_if_callback_not_registered, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_release_ase_on_acl_disconnection, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_release_ase_pair_on_acl_disconnection, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_recv_in_streaming_state, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_recv_in_enabling_state, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_cis_link_loss_in_streaming_state, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_cis_link_loss_in_disabling_state_v1, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_cis_link_loss_in_disabling_state_v2, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_cis_link_loss_in_enabling_state, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_cis_link_loss_in_enabling_state_client_retries, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_test_suite__test_ase_state_notification_retry, bt_test_ascs_test_suite_before, bt_test_ascs_test_suite_after),
};
BT_TEST_SUITE(ascs_test_suite, test_cases_ascs_test_suite, bt_test_ascs_test_suite_setup, bt_test_ascs_test_suite_teardown);
