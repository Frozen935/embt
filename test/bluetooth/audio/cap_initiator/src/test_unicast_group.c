#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_05ae85(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_05ae85(const struct bt_test_case_info *, void *);

#include <embt/base/log.h>

/* test_unicast_group.c - unit test for unicast group functions */

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
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/bap_lc3_preset.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/iso.h>
#include <fff.h>
#include <embt/base/slist.h>
#include <embt/base/util.h>
#include <embt/compiler.h>
#include <sys/errno.h>

#include "audio/bap_endpoint.h"
#include "test_common.h"

struct cap_initiator_test_unicast_group_fixture {
	struct bt_cap_unicast_group_param *group_param;
	struct bt_cap_unicast_group *unicast_group;
	struct bt_bap_qos_cfg *qos_cfg;
	void *cap_streams;
};

static void *cap_initiator_test_unicast_group_setup(void)
{
	struct cap_initiator_test_unicast_group_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_initiator_test_unicast_group_before(void *f)
{

	struct cap_initiator_test_unicast_group_fixture *fixture = f;
	struct bt_cap_unicast_group_stream_pair_param *pair_params;
	struct bt_cap_unicast_group_stream_param *stream_params;
	struct bt_cap_stream *cap_streams;
	size_t pair_cnt = 0U;
	size_t str_cnt = 0U;

	memset(f, 0, sizeof(struct cap_initiator_test_unicast_group_fixture));

	fixture->group_param = calloc(sizeof(struct bt_cap_unicast_group_param), 1);
	assert_non_null(fixture->group_param);
	pair_params = calloc(sizeof(struct bt_cap_unicast_group_stream_pair_param),
			     DIV_ROUND_UP(CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT, 2U));
	assert_non_null(pair_params);
	stream_params = calloc(sizeof(struct bt_cap_unicast_group_stream_param),
			       CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT);
	assert_non_null(stream_params);
	cap_streams = calloc(sizeof(struct bt_cap_stream),
			     CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT);
	assert_non_null(cap_streams);
	fixture->qos_cfg = calloc(sizeof(struct bt_bap_qos_cfg), 1);
	assert_non_null(fixture->qos_cfg);

	*fixture->qos_cfg = BT_BAP_QOS_CFG_UNFRAMED(10000u, 40u, 2u, 10u, 40000u); /* 16_2_1 */

	while (str_cnt < CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT) {
		stream_params[str_cnt].stream = &cap_streams[str_cnt];
		stream_params[str_cnt].qos_cfg = fixture->qos_cfg;

		if (str_cnt & 1) {
			pair_params[pair_cnt].tx_param = &stream_params[str_cnt];
		} else {
			pair_params[pair_cnt].rx_param = &stream_params[str_cnt];
		}

		str_cnt++;
		pair_cnt = str_cnt / 2U;
	}

	fixture->cap_streams = cap_streams;
	fixture->group_param->packing = BT_ISO_PACKING_SEQUENTIAL;
	fixture->group_param->params_count = pair_cnt;
	fixture->group_param->params = pair_params;
}

static void cap_initiator_test_unicast_group_after(void *f)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = f;
	struct bt_cap_unicast_group_param *group_param;
	int err;

	/* In the case of a test failing, we delete the group so that subsequent tests won't fail */
	if (fixture->unicast_group != NULL) {
		err = bt_cap_unicast_group_delete(fixture->unicast_group);
		if (err != 0) {
			bt_log_print("Failed to delete unicast group (err %d)\n", err);
		}
	}

	group_param = fixture->group_param;

	free(fixture->cap_streams);
	free(group_param->params[0].rx_param);
	free(group_param->params);
	free(group_param);
	free(fixture->qos_cfg);
}

static void cap_initiator_test_unicast_group_teardown(void *f)
{
	free(f);
}



static void cap_initiator_test_unicast_group__test_initiator_unicast_group_create(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(fixture->unicast_group);
	assert_int_equal(err, 0);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_null_param(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(NULL, &fixture->unicast_group);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_null_rx_stream(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	if (fixture->group_param->params[0].rx_param->stream == NULL) {
		skip();
	}
	fixture->group_param->params[0].rx_param->stream = NULL;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_null_tx_stream(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	if (fixture->group_param->params[0].tx_param->stream == NULL) {
		skip();
	}
	fixture->group_param->params[0].tx_param->stream = NULL;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_too_many_streams(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	fixture->group_param->params_count = CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT + 1;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_reconfig(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_reconfig(fixture->unicast_group, fixture->group_param);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(fixture->unicast_group);
	assert_int_equal(err, 0);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_reconfig_inval_null_group(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_reconfig(NULL, fixture->group_param);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_reconfig_inval_null_param(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_reconfig(fixture->unicast_group, NULL);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	struct bt_cap_stream stream = {0};
	struct bt_cap_unicast_group_stream_param stream_param = {
		.stream = &stream,
		.qos_cfg = fixture->qos_cfg,
	};
	const struct bt_cap_unicast_group_stream_pair_param pair_param = {
		.rx_param = &stream_param,
	};
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_add_streams(fixture->unicast_group, &pair_param, 1);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(fixture->unicast_group);
	assert_int_equal(err, 0);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams_inval_null_group(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	struct bt_cap_stream stream = {0};
	struct bt_cap_unicast_group_stream_param stream_param = {
		.stream = &stream,
		.qos_cfg = fixture->qos_cfg,
	};
	const struct bt_cap_unicast_group_stream_pair_param pair_param = {
		.rx_param = &stream_param,
	};
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_add_streams(NULL, &pair_param, 1);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams_inval_null_param(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_add_streams(fixture->unicast_group, NULL, 1);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams_inval_0_param(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	struct bt_cap_stream stream = {0};
	struct bt_cap_unicast_group_stream_param stream_param = {
		.stream = &stream,
		.qos_cfg = fixture->qos_cfg,
	};
	const struct bt_cap_unicast_group_stream_pair_param pair_param = {
		.rx_param = &stream_param,
	};
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_add_streams(fixture->unicast_group, &pair_param, 0);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_delete(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(fixture->unicast_group);
	assert_int_equal(err, 0);
	fixture->unicast_group = NULL;
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_delete_inval_null_group(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(NULL);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_delete_inval_double_delete(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err = 0;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_delete(fixture->unicast_group);
	assert_int_equal(err, -EINVAL);
	fixture->unicast_group = NULL;
}

static bool unicast_group_foreach_stream_cb(struct bt_cap_stream *cap_stream, void *user_data)
{
	size_t *cnt = user_data;

	ARG_UNUSED(cap_stream);

	(*cnt)++;

	return true;
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	size_t expect_cnt = 0U;
	size_t cnt = 0U;
	int err;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_foreach_stream(fixture->unicast_group,
						  unicast_group_foreach_stream_cb, &cnt);
	assert_int_equal(err, 0);

	for (size_t i = 0U; i < fixture->group_param->params_count; i++) {
		if (fixture->group_param->params[i].rx_param != NULL) {
			expect_cnt++;
		}

		if (fixture->group_param->params[i].tx_param != NULL) {
			expect_cnt++;
		}
	}

	assert_int_equal(cnt, expect_cnt);
}

static bool unicast_group_foreach_stream_return_early_cb(struct bt_cap_stream *stream,
							 void *user_data)
{
	size_t *cnt = user_data;

	ARG_UNUSED(stream);

	(*cnt)++;

	return false;
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream_return_early(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	size_t cnt = 0U;
	int err;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_foreach_stream(
		fixture->unicast_group, unicast_group_foreach_stream_return_early_cb, &cnt);
	assert_int_equal(err, -ECANCELED);
	assert_int_equal(cnt, 1U);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream_inval_null_group(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	size_t expect_cnt = 0U;
	size_t cnt = 0U;
	int err;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_foreach_stream(NULL, unicast_group_foreach_stream_cb, &cnt);
	assert_int_equal(err, -EINVAL);

	assert_int_equal(cnt, expect_cnt);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream_inval_null_func(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	size_t expect_cnt = 0U;
	size_t cnt = 0U;
	int err;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_foreach_stream(fixture->unicast_group, NULL, &cnt);
	assert_int_equal(err, -EINVAL);

	assert_int_equal(cnt, expect_cnt);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_get_info(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	struct bt_cap_unicast_group_info cap_info;
	int err;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_get_info(fixture->unicast_group, &cap_info);
	assert_int_equal(err, 0);

	assert_non_null(cap_info.unicast_group);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_get_info_inval_null_group(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	struct bt_cap_unicast_group_info cap_info;
	int err;

	err = bt_cap_unicast_group_get_info(NULL, &cap_info);
	assert_int_equal(err, -EINVAL);
}

static void cap_initiator_test_unicast_group__test_initiator_unicast_group_get_info_inval_null_info(void **cm_state)
{
	struct cap_initiator_test_unicast_group_fixture *fixture = *cm_state;
	int err;

	err = bt_cap_unicast_group_create(fixture->group_param, &fixture->unicast_group);
	assert_int_equal(err, 0);

	err = bt_cap_unicast_group_get_info(fixture->unicast_group, NULL);
	assert_int_equal(err, -EINVAL);
}

int bt_test_cap_initiator_test_unicast_group_setup(void **state)
{
	int environment_result = bt_test_audio_cap_initiator_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_initiator_test_unicast_group_setup();
	return 0;
}
static int bt_test_cap_initiator_test_unicast_group_before(void **state)
{
	(void)state;
	cap_initiator_test_unicast_group_before(*state);
	mock_init_rule_before_05ae85(NULL, *state);
	return 0;
}
int bt_test_cap_initiator_test_unicast_group_after(void **state)
{
	(void)state;
	cap_initiator_test_unicast_group_after(*state);
	mock_destroy_rule_after_05ae85(NULL, *state);
	return 0;
}
int bt_test_cap_initiator_test_unicast_group_teardown(void **state)
{
	(void)state;
	cap_initiator_test_unicast_group_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_initiator_test_unicast_group[] = {
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_create, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_null_param, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_null_rx_stream, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_null_tx_stream, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_create_inval_too_many_streams, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_reconfig, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_reconfig_inval_null_group, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_reconfig_inval_null_param, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams_inval_null_group, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams_inval_null_param, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_add_streams_inval_0_param, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_delete, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_delete_inval_null_group, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_delete_inval_double_delete, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream_return_early, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream_inval_null_group, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_foreach_stream_inval_null_func, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_get_info, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_get_info_inval_null_group, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
	cmocka_unit_test_setup_teardown(cap_initiator_test_unicast_group__test_initiator_unicast_group_get_info_inval_null_info, bt_test_cap_initiator_test_unicast_group_before, bt_test_cap_initiator_test_unicast_group_after),
};
BT_TEST_SUITE(cap_initiator_test_unicast_group, test_cases_cap_initiator_test_unicast_group, bt_test_cap_initiator_test_unicast_group_setup, bt_test_cap_initiator_test_unicast_group_teardown);
