#include "support/test_support.h"
/*
 * Copyright (c) 2023 Codecoup
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOCKS_ASCS_EXPECTS_H_
#define MOCKS_ASCS_EXPECTS_H_

#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/conn.h>
#include "ascs.h"
#include "expects_util.h"

static inline void expect_bt_ascs_cb_config_called(unsigned int expected_count,
						   struct bt_conn *conns[], struct bt_bap_ep *eps[],
						   enum bt_audio_dir dirs[], void *codec[])
{
	const char *func_name = "bt_ascs_cb.config";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_config_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_config_fake.call_count; i++) {
		assert_ptr_equal(conns[i], mock_ascs_cb_config_fake.arg0_history[i]);
		if (eps != NULL) {
			assert_ptr_equal(eps[i], mock_ascs_cb_config_fake.arg1_history[i]);
		}
		assert_int_equal(dirs[i], mock_ascs_cb_config_fake.arg2_history[i]);
	}

	if (codec != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}
}

static inline void expect_bt_ascs_cb_reconfig_called(unsigned int expected_count,
						     struct bt_bap_stream *streams[],
						     const enum bt_audio_dir dirs[], void *codec[])
{
	const char *func_name = "bt_ascs_cb.reconfig";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_reconfig_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_reconfig_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_reconfig_fake.arg0_history[i]);
		assert_int_equal(dirs[i], mock_ascs_cb_reconfig_fake.arg1_history[i]);
	}

	if (codec != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}
}

static inline void expect_bt_ascs_cb_qos_called(unsigned int expected_count,
						struct bt_bap_stream *streams[], void *qos[])
{
	const char *func_name = "bt_ascs_cb.qos";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_qos_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_qos_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_qos_fake.arg0_history[i]);
	}

	if (qos != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}
}

static inline void expect_bt_ascs_cb_enable_called(unsigned int expected_count,
						   struct bt_bap_stream *streams[], void *meta[],
						   void *meta_len[])
{
	const char *func_name = "bt_ascs_cb.enable";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_enable_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_enable_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_enable_fake.arg0_history[i]);
	}

	if (meta != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}

	if (meta_len != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}
}

static inline void expect_bt_ascs_cb_metadata_called(unsigned int expected_count,
						     struct bt_bap_stream *streams[], void *meta[],
						     void *meta_len[])
{
	const char *func_name = "bt_ascs_cb.metadata";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_metadata_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_metadata_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_metadata_fake.arg0_history[i]);
	}

	if (meta != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}

	if (meta_len != NULL) {
		/* TODO */
		fail_msg("Not implemented");
	}
}

static inline void expect_bt_ascs_cb_disable_called(unsigned int expected_count,
						    struct bt_bap_stream *streams[])
{
	const char *func_name = "bt_ascs_cb.disable";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_disable_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_disable_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_disable_fake.arg0_history[i]);
	}
}

static inline void expect_bt_ascs_cb_release_called(unsigned int expected_count,
						    struct bt_bap_stream *streams[])
{
	const char *func_name = "bt_ascs_cb.release";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_release_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_release_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_release_fake.arg0_history[i]);
	}
}

static inline void expect_bt_ascs_cb_start_called(unsigned int expected_count,
						  struct bt_bap_stream *streams[])
{
	const char *func_name = "bt_ascs_cb.start";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_start_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_start_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_start_fake.arg0_history[i]);
	}
}

static inline void expect_bt_ascs_cb_stop_called(unsigned int expected_count,
						 struct bt_bap_stream *streams[])
{
	const char *func_name = "bt_ascs_cb.stop";

	zexpect_call_count(func_name, expected_count, mock_ascs_cb_stop_fake.call_count);

	for (unsigned int i = 0U; i < mock_ascs_cb_stop_fake.call_count; i++) {
		assert_ptr_equal(streams[i], mock_ascs_cb_stop_fake.arg0_history[i]);
	}
}

#endif /* MOCKS_ASCS_EXPECTS_H_ */
