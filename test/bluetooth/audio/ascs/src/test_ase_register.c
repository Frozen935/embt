#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_26f0f3(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_26f0f3(const struct bt_test_case_info *, void *);


/* main.c - Application main entry point */

/*
 * Copyright (c) 2024 Demant A/S
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/audio/ascs.h>
#include <fff.h>

#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/pacs.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/uuid.h>
#include <embt/base/util_macro.h>
#include <embt/compiler.h>
#include "ascs.h"
#include "bap_stream.h"

#include "test_common.h"

static void ascs_register_test_suite_after(void *f)
{
	ARG_UNUSED(f);

	(void)bt_ascs_unregister();
}



static void ascs_register_test_suite__test_ascs_register_with_null_param(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_ascs_register(NULL);
	assert_int_equal(err, -EINVAL);
}

static void ascs_register_test_suite__test_ascs_register_twice(void **cm_state)
{
	(void)cm_state;
	int err;
	struct bt_ascs_register_param param = {
		CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT,
		CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT,
	};

	/* Setup already registered once, so calling once here should be sufficient */
	err = bt_ascs_register(&param);
	assert_int_equal(err, 0);

	/* Setup already registered once, so calling once here should be sufficient */
	err = bt_ascs_register(&param);
	assert_int_equal(err, -EALREADY);

	err = bt_ascs_unregister();
	assert_int_equal(err, 0);
}

static void ascs_register_test_suite__test_ascs_register_too_many_sinks(void **cm_state)
{
	(void)cm_state;
	int err;
	struct bt_ascs_register_param param = {
		CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT + 1,
		CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT,
	};

	err = bt_ascs_register(&param);
	assert_int_equal(err, -EINVAL);
}

static void ascs_register_test_suite__test_ascs_register_too_many_sources(void **cm_state)
{
	(void)cm_state;
	int err;
	struct bt_ascs_register_param param = {
		CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT,
		CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1,
	};

	err = bt_ascs_register(&param);
	assert_int_equal(err, -EINVAL);
}

static void ascs_register_test_suite__test_ascs_register_zero_ases(void **cm_state)
{
	(void)cm_state;
	int err;
	struct bt_ascs_register_param param = {0, 0};

	err = bt_ascs_register(&param);
	assert_int_equal(err, -EINVAL);
}

static void ascs_register_test_suite__test_ascs_register_fewer_than_max_ases(void **cm_state)
{
	(void)cm_state;
	int err;
	struct bt_ascs_register_param param = {
		CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT > 0 ? CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT - 1 : 0,
		CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT > 0 ? CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT - 1 : 0,
	};

	err = bt_ascs_register(&param);
	assert_int_equal(err, 0);
}

static void ascs_register_test_suite__test_ascs_unregister_without_register(void **cm_state)
{
	(void)cm_state;
	int err;

	err = bt_ascs_unregister();
	assert_int_equal(err, -EALREADY);
}


static int bt_test_ascs_register_test_suite_before(void **state)
{
	(void)state;
	mock_init_rule_before_26f0f3(NULL, *state);
	return 0;
}
int bt_test_ascs_register_test_suite_after(void **state)
{
	(void)state;
	ascs_register_test_suite_after(*state);
	mock_destroy_rule_after_26f0f3(NULL, *state);
	return 0;
}

static const struct CMUnitTest test_cases_ascs_register_test_suite[] = {
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_register_with_null_param, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_register_twice, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_register_too_many_sinks, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_register_too_many_sources, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_register_zero_ases, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_register_fewer_than_max_ases, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
	cmocka_unit_test_setup_teardown(ascs_register_test_suite__test_ascs_unregister_without_register, bt_test_ascs_register_test_suite_before, bt_test_ascs_register_test_suite_after),
};
BT_TEST_SUITE(ascs_register_test_suite, test_cases_ascs_register_test_suite, bt_test_audio_ascs_environment_setup, NULL);
