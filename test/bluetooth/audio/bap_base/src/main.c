#include "support/test_support.h"

/* main.c - Application main entry point */

/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/compiler.h>
#include <fff.h>

DEFINE_FFF_GLOBALS;

struct bap_base_test_suite_fixture {
	struct bt_data valid_base_ad;
	uint8_t *valid_base_data;
	struct bt_data invalid_base_ad;
	uint8_t *invalid_base_data;
};

static void bap_base_test_suite_fixture_init(struct bap_base_test_suite_fixture *fixture)
{
	uint8_t base_data[] = {
		0x51U, 0x18U,                      /* uuid */
		0x40U, 0x9CU, 0x00U,               /* pd */
		0x02U,                             /* subgroup count */
		0x01U,                             /* Subgroup 1: bis count */
		0x06U, 0x00U, 0x00U, 0x00U, 0x00U, /* LC3 codec_id*/
		0x10U,                             /* cc length */
		0x02U, 0x01U, 0x03U, 0x02U,
		0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U,
		0x03U, 0x04U, 0x28U, 0x00U,        /* cc */
		0x04U,                             /* meta length */
		0x03U, 0x02U, 0x01U, 0x00U,        /* meta */
		0x01U,                             /* bis index */
		0x03U,                             /* bis cc length */
		0x02U, 0x03U, 0x03U,               /* bis cc length */
		0x01U,                             /* Subgroup 1: bis count */
		0x06U, 0x00U, 0x00U, 0x00U, 0x00U, /* LC3 codec_id*/
		0x10U,                             /* cc length */
		0x02U, 0x01U, 0x03U, 0x02U,
		0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U,
		0x03U, 0x04U, 0x28U, 0x00U,        /* cc */
		0x04U,                             /* meta length */
		0x03U, 0x02U, 0x01U, 0x00U,        /* meta */
		0x02U,                             /* bis index */
		0x03U,                             /* bis cc length */
		0x02U, 0x03U, 0x03U                /* bis cc length */
	};

	fixture->valid_base_data = malloc(sizeof(base_data));
	assert_non_null(fixture->valid_base_data);
	memcpy(fixture->valid_base_data, base_data, sizeof(base_data));

	fixture->valid_base_ad.type = 0x16U; /* service data */
	fixture->valid_base_ad.data_len = sizeof(base_data);
	fixture->valid_base_ad.data = fixture->valid_base_data;

	/* Modify the CC length to generate an invalid BASE for invalid BASE tests */
	base_data[12] = 0xaa; /* Set invalid CC length*/
	fixture->invalid_base_data = malloc(sizeof(base_data));
	assert_non_null(fixture->invalid_base_data);
	memcpy(fixture->invalid_base_data, base_data, sizeof(base_data));

	fixture->invalid_base_ad.type = 0x16U; /* service data */
	fixture->invalid_base_ad.data_len = sizeof(base_data);
	fixture->invalid_base_ad.data = fixture->invalid_base_data;
}

static void *bap_base_test_suite_setup(void)
{
	struct bap_base_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void bap_base_test_suite_before(void *f)
{
	memset(f, 0, sizeof(struct bap_base_test_suite_fixture));
	bap_base_test_suite_fixture_init(f);
}

static void bap_base_test_suite_after(void *f)
{
	struct bap_base_test_suite_fixture *fixture = f;

	free(fixture->valid_base_data);
	free(fixture->invalid_base_data);
}

static void bap_base_test_suite_teardown(void *f)
{
	free(f);
}



static void bap_base_test_suite__test_base_get_base_from_ad(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);

	assert_non_null(base);
}

static void bap_base_test_suite__test_base_get_base_from_ad_inval_base(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->invalid_base_ad);

	assert_null(base);
}

static void bap_base_test_suite__test_base_get_base_from_ad_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(NULL);

	assert_null(base);
}

static void bap_base_test_suite__test_base_get_base_from_ad_inval_param_type(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base;

	fixture->valid_base_ad.type = 0x03U; /* BT_DATA_UUID16_ALL */

	base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);

	assert_null(base);
}

static void bap_base_test_suite__test_base_get_base_from_ad_inval_param_len(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base;

	fixture->valid_base_ad.data_len = 0x03U; /* Minimum len is BASE_MIN_SIZE (16) */

	base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);

	assert_null(base);
}

static void bap_base_test_suite__test_base_get_base_from_ad_inval_param_uuid(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base;

	/* Modify the BASE data to have invalid UUID */
	fixture->valid_base_data[0] = 0x01;
	fixture->valid_base_data[1] = 0x02;

	base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);

	assert_null(base);
}

static void bap_base_test_suite__test_base_get_size(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_get_size(base);
	assert_int_equal(ret, 70);
}

static void bap_base_test_suite__test_base_get_size_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	int ret;

	ret = bt_bap_base_get_size(NULL);
	assert_int_equal(ret, -EINVAL);
}

static void bap_base_test_suite__test_base_get_pres_delay(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_get_pres_delay(base);
	assert_int_equal(ret, 40000);
}

static void bap_base_test_suite__test_base_get_pres_delay_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	int ret;

	ret = bt_bap_base_get_pres_delay(NULL);
	assert_int_equal(ret, -EINVAL);
}

static void bap_base_test_suite__test_base_get_subgroup_count(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_get_subgroup_count(base);
	assert_int_equal(ret, 2);
}

static void bap_base_test_suite__test_base_get_subgroup_count_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	int ret;

	ret = bt_bap_base_get_subgroup_count(NULL);
	assert_int_equal(ret, -EINVAL);
}

static void bap_base_test_suite__test_base_get_bis_indexes(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	uint32_t bis_indexes;
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_get_bis_indexes(base, &bis_indexes);
	assert_int_equal(ret, 0);
	assert_int_equal(bis_indexes, 0x00000003U /* Bit 1 and 2 */);
}

static void bap_base_test_suite__test_base_get_bis_indexes_inval_param_null_base(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	uint32_t bis_indexes;
	int ret;

	ret = bt_bap_base_get_bis_indexes(NULL, &bis_indexes);
	assert_int_equal(ret, -EINVAL);
}

static void bap_base_test_suite__test_base_get_bis_indexes_inval_param_null_index(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_get_bis_indexes(base, NULL);
	assert_int_equal(ret, -EINVAL);
}

static bool test_base_foreach_subgroup_cb(const struct bt_bap_base_subgroup *subgroup,
					  void *user_data)
{
	size_t *count = user_data;

	ARG_UNUSED(subgroup);

	(*count)++;

	return true;
}

static void bap_base_test_suite__test_base_foreach_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	size_t count = 0U;
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_foreach_subgroup_cb, &count);
	assert_int_equal(ret, 0);
	assert_int_equal(count, 0x02U);
}

static void bap_base_test_suite__test_base_foreach_subgroup_inval_param_null_base(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	size_t count;
	int ret;

	ret = bt_bap_base_foreach_subgroup(NULL, test_base_foreach_subgroup_cb, &count);
	assert_int_equal(ret, -EINVAL);
}

static void bap_base_test_suite__test_base_foreach_subgroup_inval_param_null_cb(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, NULL, NULL);
	assert_int_equal(ret, -EINVAL);
}

static bool test_base_get_subgroup_codec_id_cb(const struct bt_bap_base_subgroup *subgroup,
					       void *user_data)
{
	struct bt_bap_base_codec_id codec_id;
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_id(subgroup, &codec_id);
	assert_int_equal(ret, 0);
	assert_int_equal(codec_id.id, 0x06U);
	assert_int_equal(codec_id.cid, 0x0000U);
	assert_int_equal(codec_id.vid, 0x0000U);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_id(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_get_subgroup_codec_id_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_codec_id_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	struct bt_bap_base_codec_id codec_id;
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_id(NULL, &codec_id);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_id_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_codec_id_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_base_get_subgroup_codec_id_inval_param_null_cb(const struct bt_bap_base_subgroup *subgroup,
						    void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_id(subgroup, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_id_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_codec_id_inval_param_null_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_codec_data_cb(const struct bt_bap_base_subgroup *subgroup,
						 void *user_data)
{
	const uint8_t expected_data[] = {
		0x02U, 0x01U, 0x03U, 0x02U, 0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U, 0x03U, 0x04U, 0x28U, 0x00U,
	};
	uint8_t *data;
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_data(subgroup, &data);
	assert_int_equal(ret, sizeof(expected_data));
	assert_memory_equal(data, expected_data, sizeof(expected_data));

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_data(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_get_subgroup_codec_data_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_codec_data_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	uint8_t *data;
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_data(NULL, &data);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_data_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_codec_data_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_base_get_subgroup_codec_data_inval_param_null_cb(const struct bt_bap_base_subgroup *subgroup,
						      void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_data(subgroup, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_data_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_codec_data_inval_param_null_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_codec_meta_cb(const struct bt_bap_base_subgroup *subgroup,
						 void *user_data)
{
	const uint8_t expected_data[] = {0x03U, 0x02U, 0x01U, 0x00U};
	uint8_t *data;
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_meta(subgroup, &data);
	assert_int_equal(ret, sizeof(expected_data));
	assert_memory_equal(data, expected_data, sizeof(expected_data));

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_meta(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_get_subgroup_codec_meta_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_codec_meta_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	uint8_t *data;
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_meta(NULL, &data);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_meta_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_codec_meta_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_base_get_subgroup_codec_meta_inval_param_null_cb(const struct bt_bap_base_subgroup *subgroup,
						      void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_codec_meta(subgroup, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_codec_meta_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_codec_meta_inval_param_null_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_subgroup_codec_to_codec_cfg_cb(const struct bt_bap_base_subgroup *subgroup,
						     void *user_data)
{
	const uint8_t expected_meta[] = {0x03U, 0x02U, 0x01U, 0x00U};
	const uint8_t expected_data[] = {
		0x02U, 0x01U, 0x03U, 0x02U, 0x02U, 0x01U, 0x05U, 0x03U,
		0x01U, 0x00U, 0x00U, 0x00U, 0x03U, 0x04U, 0x28U, 0x00U,
	};
	struct bt_audio_codec_cfg codec_cfg;
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_codec_to_codec_cfg(subgroup, &codec_cfg);
	assert_int_equal(ret, 0);
	assert_int_equal(codec_cfg.data_len, sizeof(expected_data));
	assert_int_equal(codec_cfg.meta_len, sizeof(expected_meta));
	assert_memory_equal(codec_cfg.data, expected_data, sizeof(expected_data));
	assert_memory_equal(codec_cfg.meta, expected_meta, sizeof(expected_meta));

	return true;
}

static void bap_base_test_suite__test_base_subgroup_codec_to_codec_cfg(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_subgroup_codec_to_codec_cfg_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_subgroup_codec_to_codec_cfg_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	struct bt_audio_codec_cfg codec_cfg;
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_codec_to_codec_cfg(NULL, &codec_cfg);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_codec_to_codec_cfg_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_subgroup_codec_to_codec_cfg_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_subgroup_codec_to_codec_cfg_inval_param_null_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_codec_to_codec_cfg(subgroup, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_codec_to_codec_cfg_inval_param_null(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_subgroup_codec_to_codec_cfg_inval_param_null_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_bis_count_cb(const struct bt_bap_base_subgroup *subgroup,
						void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_bis_count(subgroup);
	assert_int_equal(ret, 0x01);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_bis_count(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_get_subgroup_bis_count_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_get_subgroup_bis_count_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_get_subgroup_bis_count(NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_get_subgroup_bis_count_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_get_subgroup_bis_count_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_bt_bap_base_subgroup_get_bis_indexes_cb(const struct bt_bap_base_subgroup *subgroup,
					     void *user_data)
{
	uint32_t bis_indexes;
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_get_bis_indexes(subgroup, &bis_indexes);
	assert_int_equal(ret, 0);
	assert_int_not_equal(bis_indexes, 0U /* May be Bit 1 or 2 */);

	return true;
}

static void bap_base_test_suite__test_bt_bap_base_subgroup_get_bis_indexes(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_bt_bap_base_subgroup_get_bis_indexes_cb,
					   NULL);
	assert_int_equal(ret, 0);
}

static bool test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	uint32_t bis_indexes;
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_get_bis_indexes(NULL, &bis_indexes);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_index_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_get_bis_indexes(subgroup, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_index(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_index_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_base_subgroup_foreach_bis_subgroup_bis_cb(const struct bt_bap_base_subgroup_bis *bis,
					       void *user_data)
{
	size_t *count = user_data;

	ARG_UNUSED(bis);

	(*count)++;

	return true;
}

static bool test_base_subgroup_foreach_bis_subgroup_cb(const struct bt_bap_base_subgroup *subgroup,
						       void *user_data)
{
	size_t *total_count = user_data;
	size_t count = 0U;
	int ret;

	ret = bt_bap_base_subgroup_foreach_bis(
		subgroup, test_base_subgroup_foreach_bis_subgroup_bis_cb, &count);
	assert_int_equal(ret, 0);
	assert_int_equal(count, 0x01U);

	*total_count += count;

	return true;
}

static void bap_base_test_suite__test_base_subgroup_foreach_bis(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	size_t count = 0U;
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(base, test_base_subgroup_foreach_bis_subgroup_cb,
					   &count);
	assert_int_equal(ret, 0);
	assert_int_equal(count, 0x02U);
}

static bool test_base_subgroup_foreach_bis_inval_param_null_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	size_t count;
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_foreach_bis(NULL, test_base_subgroup_foreach_bis_subgroup_bis_cb,
					       &count);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_subgroup(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_subgroup_foreach_bis_inval_param_null_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_base_subgroup_foreach_bis_inval_param_null_cb_cb(const struct bt_bap_base_subgroup *subgroup,
						      void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_foreach_bis(subgroup, NULL, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_cb(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_subgroup_foreach_bis_inval_param_null_cb_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool
test_base_subgroup_bis_codec_to_codec_cfg_bis_cb(const struct bt_bap_base_subgroup_bis *bis,
						 void *user_data)
{
	const uint8_t expected_data[] = {0x02U, 0x03U, 0x03U};
	struct bt_audio_codec_cfg codec_cfg;
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_bis_codec_to_codec_cfg(bis, &codec_cfg);
	assert_int_equal(ret, 0);
	assert_int_equal(codec_cfg.data_len, sizeof(expected_data));
	assert_memory_equal(codec_cfg.data, expected_data, sizeof(expected_data));

	return true;
}

static bool
test_base_subgroup_bis_codec_to_codec_cfg_subgroup_cb(const struct bt_bap_base_subgroup *subgroup,
						      void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_foreach_bis(
		subgroup, test_base_subgroup_bis_codec_to_codec_cfg_bis_cb, NULL);
	assert_int_equal(ret, 0);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_bis_codec_to_codec_cfg(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_subgroup_bis_codec_to_codec_cfg_subgroup_cb, NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_bis_bis_cb(
	const struct bt_bap_base_subgroup_bis *bis, void *user_data)
{
	struct bt_audio_codec_cfg codec_cfg;
	int ret;

	ARG_UNUSED(bis);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_bis_codec_to_codec_cfg(NULL, &codec_cfg);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static bool test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_bis_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_foreach_bis(
		NULL, test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_bis_bis_cb, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_bis(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base, test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_bis_subgroup_cb,
		NULL);
	assert_int_equal(ret, 0);
}

static bool test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_codec_cfg_bis_cb(
	const struct bt_bap_base_subgroup_bis *bis, void *user_data)
{
	int ret;

	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_bis_codec_to_codec_cfg(bis, NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static bool test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_codec_cfg_subgroup_cb(
	const struct bt_bap_base_subgroup *subgroup, void *user_data)
{
	int ret;

	ARG_UNUSED(subgroup);
	ARG_UNUSED(user_data);

	ret = bt_bap_base_subgroup_foreach_bis(
		NULL, test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_codec_cfg_bis_cb,
		NULL);
	assert_int_equal(ret, -EINVAL);

	return true;
}

static void bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_codec_cfg(void **cm_state)
{
	struct bap_base_test_suite_fixture *fixture = *cm_state;
	const struct bt_bap_base *base = bt_bap_base_get_base_from_ad(&fixture->valid_base_ad);
	int ret;

	assert_non_null(base);

	ret = bt_bap_base_foreach_subgroup(
		base,
		test_base_subgroup_bis_codec_to_codec_cfg_inval_param_null_codec_cfg_subgroup_cb,
		NULL);
	assert_int_equal(ret, 0);
}

static int bt_test_bap_base_test_suite_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = bap_base_test_suite_setup();
	return 0;
}
static int bt_test_bap_base_test_suite_before(void **state)
{
	(void)state;
	bap_base_test_suite_before(*state);
	return 0;
}
static int bt_test_bap_base_test_suite_after(void **state)
{
	(void)state;
	bap_base_test_suite_after(*state);
	return 0;
}
static int bt_test_bap_base_test_suite_teardown(void **state)
{
	(void)state;
	bap_base_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_bap_base_test_suite[] = {
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_base_from_ad, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_base_from_ad_inval_base, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_base_from_ad_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_base_from_ad_inval_param_type, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_base_from_ad_inval_param_len, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_base_from_ad_inval_param_uuid, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_size, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_size_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_pres_delay, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_pres_delay_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_count, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_count_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_bis_indexes, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_bis_indexes_inval_param_null_base, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_bis_indexes_inval_param_null_index, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_foreach_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_foreach_subgroup_inval_param_null_base, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_foreach_subgroup_inval_param_null_cb, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_id, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_id_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_id_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_data, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_data_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_data_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_meta, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_meta_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_codec_meta_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_codec_to_codec_cfg, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_codec_to_codec_cfg_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_codec_to_codec_cfg_inval_param_null, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_bis_count, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_get_subgroup_bis_count_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_bt_bap_base_subgroup_get_bis_indexes, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_bt_bap_base_subgroup_get_bis_indexes_inval_param_null_index, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_foreach_bis, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_subgroup, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_cb, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_bis_codec_to_codec_cfg, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_bis, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
	cmocka_unit_test_setup_teardown(bap_base_test_suite__test_base_subgroup_foreach_bis_inval_param_null_codec_cfg, bt_test_bap_base_test_suite_before, bt_test_bap_base_test_suite_after),
};
BT_TEST_SUITE(bap_base_test_suite, test_cases_bap_base_test_suite, bt_test_bap_base_test_suite_setup, bt_test_bap_base_test_suite_teardown);
