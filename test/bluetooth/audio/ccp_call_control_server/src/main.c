#include "support/test_support.h"
#include "test_environment.h"

/* main.c - Application main entry point */

/*
 * Copyright (c) 2024-2026 Nordic Semiconductor ASA
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
#include <embt/bluetooth/audio/ccp.h>
#include <embt/bluetooth/audio/tbs.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/uuid.h>
#include <fff.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
DEFINE_FFF_GLOBALS;

#define DEFAULT_BEARER_NAME "test"
#define DEFAULT_BEARER_UCI  "un999"
#define DEFAULT_BEARER_TECH BT_BEARER_TECH_3G
#define DEFAULT_BEARER_URI_SCHEMES "tel,skype"

struct ccp_call_control_server_test_suite_fixture {
	/** Need 1 additional bearer than the max to trigger some corner cases */
	struct bt_ccp_call_control_server_bearer
		*bearers[CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT + 1];
};

static void *ccp_call_control_server_test_suite_setup(void)
{
	struct ccp_call_control_server_test_suite_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void ccp_call_control_server_test_suite_before(void *f)
{
	memset(f, 0, sizeof(struct ccp_call_control_server_test_suite_fixture));
}

static void ccp_call_control_server_test_suite_after(void *f)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = f;

	/* We unregister from largest to lowest index, as GTBS shall be unregistered last and is
	 * always at index 0
	 */
	for (size_t i = ARRAY_SIZE(fixture->bearers); i > 0; i--) {
		/* Since size_t cannot be negative, we cannot use the regular i >= 0 when counting
		 * downwards as that will always be true, so we use an additional index variable
		 */
		const size_t index_to_unreg = i - 1;

		if (fixture->bearers[index_to_unreg] != NULL) {
			bt_ccp_call_control_server_unregister_bearer(
				fixture->bearers[index_to_unreg]);
		}

		fixture->bearers[index_to_unreg] = NULL;
	}
}

static void ccp_call_control_server_test_suite_teardown(void *f)
{
	free(f);
}



static void register_default_bearer(struct ccp_call_control_server_test_suite_fixture *fixture)
{
	const struct bt_tbs_register_param register_param = {
		.provider_name = DEFAULT_BEARER_NAME,
		.uci = DEFAULT_BEARER_UCI,
		.uri_schemes_supported = DEFAULT_BEARER_URI_SCHEMES,
		.gtbs = true,
		.authorization_required = false,
		.technology = DEFAULT_BEARER_TECH,
		.optional_opcodes = 0,
	};
	int err;

	err = bt_ccp_call_control_server_register_bearer(&register_param, &fixture->bearers[0]);
	assert_int_equal(err, 0);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	register_default_bearer(fixture);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_multiple_bearers(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	if (CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT == 1) {
		skip();
	}

	register_default_bearer(fixture);

	for (int i = 1; i < CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT; i++) {
		struct bt_tbs_register_param register_param = {
			.provider_name = "test",
			.uci = DEFAULT_BEARER_UCI,
			.uri_schemes_supported = DEFAULT_BEARER_URI_SCHEMES,
			.gtbs = false,
			.authorization_required = false,
			.technology = BT_BEARER_TECH_3G,
			.optional_opcodes = 0,
		};
		int err;

		err = bt_ccp_call_control_server_register_bearer(&register_param,
								 &fixture->bearers[i]);
		assert_int_equal(err, 0);
	}
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_null_param(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_server_register_bearer(NULL, &fixture->bearers[0]);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const struct bt_tbs_register_param register_param = {
		.provider_name = "test",
		.uci = DEFAULT_BEARER_UCI,
		.uri_schemes_supported = DEFAULT_BEARER_URI_SCHEMES,
		.gtbs = true,
		.authorization_required = false,
		.technology = BT_BEARER_TECH_3G,
		.optional_opcodes = 0,
	};
	int err;

	err = bt_ccp_call_control_server_register_bearer(&register_param, NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_no_gtbs(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const struct bt_tbs_register_param register_param = {
		.provider_name = "test",
		.uci = DEFAULT_BEARER_UCI,
		.uri_schemes_supported = DEFAULT_BEARER_URI_SCHEMES,
		.gtbs = false,
		.authorization_required = false,
		.technology = BT_BEARER_TECH_3G,
		.optional_opcodes = 0,
	};
	int err;

	err = bt_ccp_call_control_server_register_bearer(&register_param, &fixture->bearers[0]);
	assert_int_equal(err, -EAGAIN);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_double_gtbs(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const struct bt_tbs_register_param register_param = {
		.provider_name = "test",
		.uci = DEFAULT_BEARER_UCI,
		.uri_schemes_supported = DEFAULT_BEARER_URI_SCHEMES,
		.gtbs = true,
		.authorization_required = false,
		.technology = BT_BEARER_TECH_3G,
		.optional_opcodes = 0,
	};
	int err;

	if (CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT == 1) {
		skip();
	}

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_register_bearer(&register_param, &fixture->bearers[1]);
	assert_int_equal(err, -EALREADY);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_cnt(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const struct bt_tbs_register_param register_param = {
		.provider_name = "test",
		.uci = DEFAULT_BEARER_UCI,
		.uri_schemes_supported = DEFAULT_BEARER_URI_SCHEMES,
		.gtbs = false,
		.authorization_required = false,
		.technology = BT_BEARER_TECH_3G,
		.optional_opcodes = 0,
	};
	int err;

	if (CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT == 1) {
		skip();
	}

	register_default_bearer(fixture);

	for (int i = 1; i < CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT; i++) {

		err = bt_ccp_call_control_server_register_bearer(&register_param,
								 &fixture->bearers[i]);
		assert_int_equal(err, 0);
	}

	err = bt_ccp_call_control_server_register_bearer(
		&register_param, &fixture->bearers[CONFIG_BT_CCP_CALL_CONTROL_SERVER_BEARER_COUNT]);
	assert_int_equal(err, -ENOMEM);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_unregister_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_unregister_bearer_inval_double_unregister(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, -EALREADY);

	fixture->bearers[0] = NULL;
}

static void ccp_call_control_server_test_suite__test_ccp_call_control_server_unregister_bearer_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_ccp_call_control_server_unregister_bearer(NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 1];
	const char *new_bearer_name = "New bearer name";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_provider_name(fixture->bearers[0],
								  new_bearer_name);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_provider_name(
		fixture->bearers[0], res_bearer_name, sizeof(res_bearer_name));
	assert_int_equal(err, 0);

	assert_string_equal(new_bearer_name, res_bearer_name);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const char *new_bearer_name = "New bearer name";
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_set_bearer_provider_name(fixture->bearers[0],
								  new_bearer_name);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const char *new_bearer_name = "New bearer name";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_provider_name(NULL, new_bearer_name);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_null_name(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_provider_name(fixture->bearers[0], NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_empty_name(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const char *inval_bearer_name = "";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_provider_name(fixture->bearers[0],
								  inval_bearer_name);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_long_name(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char inval_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 2];
	int err;

	for (size_t i = 0U; i < ARRAY_SIZE(inval_bearer_name); i++) {
		inval_bearer_name[i] = 'a';
	}
	inval_bearer_name[sizeof(inval_bearer_name) - 1U] = '\0';

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_provider_name(fixture->bearers[0],
								  inval_bearer_name);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_provider_name(
		fixture->bearers[0], res_bearer_name, sizeof(res_bearer_name));
	assert_int_equal(err, 0);

	assert_string_equal(DEFAULT_BEARER_NAME, res_bearer_name);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 1];
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_provider_name(
		fixture->bearers[0], res_bearer_name, sizeof(res_bearer_name));
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_provider_name(NULL, res_bearer_name,
								  sizeof(res_bearer_name));
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_null_name(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_provider_name(fixture->bearers[0], NULL, 0);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_size_0(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_provider_name(fixture->bearers[0],
								  res_bearer_name, 0);
	assert_int_equal(err, -ENOMEM);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_small_size(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_name[CONFIG_BT_CCP_CALL_CONTROL_SERVER_PROVIDER_NAME_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_provider_name(fixture->bearers[0],
								  res_bearer_name, 1);
	assert_int_equal(err, -ENOMEM);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_uci[BT_TBS_MAX_UCI_SIZE];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uci(fixture->bearers[0], res_bearer_uci);
	assert_int_equal(err, 0);

	assert_string_equal(DEFAULT_BEARER_UCI, res_bearer_uci);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_uci[BT_TBS_MAX_UCI_SIZE];
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_uci(fixture->bearers[0], res_bearer_uci);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_bearer_uci[BT_TBS_MAX_UCI_SIZE];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uci(NULL, res_bearer_uci);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci_inval_null_uci(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uci(fixture->bearers[0], NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const enum bt_bearer_tech new_bearer_tech = BT_BEARER_TECH_3G;
	enum bt_bearer_tech res_bearer_tech;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_tech(fixture->bearers[0], new_bearer_tech);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_tech(fixture->bearers[0], &res_bearer_tech);
	assert_int_equal(err, 0);

	assert_int_equal(new_bearer_tech, res_bearer_tech);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const enum bt_bearer_tech new_bearer_tech = BT_BEARER_TECH_3G;
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_set_bearer_tech(fixture->bearers[0], new_bearer_tech);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const enum bt_bearer_tech new_bearer_tech = BT_BEARER_TECH_3G;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_tech(NULL, new_bearer_tech);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech_inval_tech(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_tech(fixture->bearers[0], 0x10);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	enum bt_bearer_tech res_bearer_tech;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_tech(fixture->bearers[0], &res_bearer_tech);
	assert_int_equal(err, 0);

	assert_int_equal(DEFAULT_BEARER_TECH, res_bearer_tech);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	enum bt_bearer_tech res_bearer_tech;
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_tech(fixture->bearers[0], &res_bearer_tech);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	enum bt_bearer_tech res_bearer_tech;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_tech(NULL, &res_bearer_tech);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech_inval_null_tech(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_tech(fixture->bearers[0], NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 1];
	/* Contains a mix of simple URIs, but also ones that use special characters like `-`, `+`,
	 * `.` and capital letters.
	 */
	const char *new_uri_schemes =
		"tel,chrome-extension,coap+tcp,xmlrpc.beep,machineProvisioningProgressReporter";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0],
								new_uri_schemes);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_uri_schemes(
		fixture->bearers[0], res_uri_schemes, sizeof(res_uri_schemes));
	assert_int_equal(err, 0);

	assert_string_equal(new_uri_schemes, res_uri_schemes);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_no_change(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 1];
	/* Contains a mix of simple URIs, but also ones that use special characters like `-`, `+`,
	 * `.` and capital letters.
	 */
	const char *same_uri_schemes = DEFAULT_BEARER_URI_SCHEMES;
	int err;

	register_default_bearer(fixture);

	/* Verify that the `same_uri_schemes` is already set */
	err = bt_ccp_call_control_server_get_bearer_uri_schemes(
		fixture->bearers[0], res_uri_schemes, sizeof(res_uri_schemes));
	assert_int_equal(err, 0);

	assert_string_equal(same_uri_schemes, res_uri_schemes);

	/* Attempt to set again */
	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0],
								same_uri_schemes);
	assert_int_equal(err, 0);

	/* Verify no change */
	err = bt_ccp_call_control_server_get_bearer_uri_schemes(
		fixture->bearers[0], res_uri_schemes, sizeof(res_uri_schemes));
	assert_int_equal(err, 0);

	assert_string_equal(same_uri_schemes, res_uri_schemes);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const char *new_uri_schemes = "tel";
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0],
								new_uri_schemes);
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const char *new_uri_schemes = "tel";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(NULL, new_uri_schemes);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_null_uri_schemes(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0], NULL);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_empty_uri_schemes(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	const char *inval_bearer_uri_schemes = "";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0],
								inval_bearer_uri_schemes);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_long_uri_schemes(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char inval_bearer_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 2];
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(inval_bearer_uri_schemes); i++) {
		inval_bearer_uri_schemes[i] = 'a';
	}
	inval_bearer_uri_schemes[ARRAY_SIZE(inval_bearer_uri_schemes) - 1] = '\0';

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0],
								inval_bearer_uri_schemes);
	assert_int_equal(err, -ENOMEM);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_inval_chars(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char *inval_bearer_uri_schemes_utf8 = "tel,🌍";
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(fixture->bearers[0],
								inval_bearer_uri_schemes_utf8);
	assert_int_equal(err, -EINVAL);

	char *inval_bearer_uri_schemes_missing_after_comma = "tel,";

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(
		fixture->bearers[0], inval_bearer_uri_schemes_missing_after_comma);
	assert_int_equal(err, -EINVAL);

	char *inval_bearer_uri_schemes_special_char = "tel,a@";

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(
		fixture->bearers[0], inval_bearer_uri_schemes_special_char);
	assert_int_equal(err, -EINVAL);

	char *inval_bearer_uri_schemes_number_first = "123,tel";

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(
		fixture->bearers[0], inval_bearer_uri_schemes_number_first);
	assert_int_equal(err, -EINVAL);

	char *inval_bearer_uri_schemes_number_first_2 = "tel,123";

	err = bt_ccp_call_control_server_set_bearer_uri_schemes(
		fixture->bearers[0], inval_bearer_uri_schemes_number_first_2);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uri_schemes(
		fixture->bearers[0], res_uri_schemes, sizeof(res_uri_schemes));
	assert_int_equal(err, 0);

	assert_string_equal(DEFAULT_BEARER_URI_SCHEMES, res_uri_schemes);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_not_registered(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 1];
	int err;

	/* Register and unregister bearer to get a valid pointer but where it is unregistered*/
	register_default_bearer(fixture);
	err = bt_ccp_call_control_server_unregister_bearer(fixture->bearers[0]);
	assert_int_equal(err, 0);

	err = bt_ccp_call_control_server_get_bearer_uri_schemes(
		fixture->bearers[0], res_uri_schemes, sizeof(res_uri_schemes));
	assert_int_equal(err, -EFAULT);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_null_bearer(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uri_schemes(NULL, res_uri_schemes,
								sizeof(res_uri_schemes));
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_null_uri_schemes(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uri_schemes(
		fixture->bearers[0], NULL,
		CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH);
	assert_int_equal(err, -EINVAL);
}

static void ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_zero_size(void **cm_state)
{
	struct ccp_call_control_server_test_suite_fixture *fixture = *cm_state;
	char res_uri_schemes[CONFIG_BT_CCP_CALL_CONTROL_SERVER_URI_SCHEMES_MAX_LENGTH + 1];
	int err;

	register_default_bearer(fixture);

	err = bt_ccp_call_control_server_get_bearer_uri_schemes(fixture->bearers[0],
								res_uri_schemes, 0U);
	assert_int_equal(err, -ENOMEM);
}

int bt_test_ccp_call_control_server_test_suite_setup(void **state)
{
	int environment_result = bt_test_audio_ccp_call_control_server_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = ccp_call_control_server_test_suite_setup();
	return 0;
}
static int bt_test_ccp_call_control_server_test_suite_before(void **state)
{
	(void)state;
	ccp_call_control_server_test_suite_before(*state);
	return 0;
}
int bt_test_ccp_call_control_server_test_suite_after(void **state)
{
	(void)state;
	ccp_call_control_server_test_suite_after(*state);
	return 0;
}
int bt_test_ccp_call_control_server_test_suite_teardown(void **state)
{
	(void)state;
	ccp_call_control_server_test_suite_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_ccp_call_control_server_test_suite[] = {
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_multiple_bearers, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_null_param, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_no_gtbs, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_double_gtbs, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_register_bearer_inval_cnt, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_unregister_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_unregister_bearer_inval_double_unregister, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_ccp_call_control_server_unregister_bearer_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_null_name, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_empty_name, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_provider_name_inval_long_name, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_null_name, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_size_0, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_provider_name_inval_small_size, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uci_inval_null_uci, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_tech_inval_tech, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_tech_inval_null_tech, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_no_change, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_null_uri_schemes, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_empty_uri_schemes, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_long_uri_schemes, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_set_bearer_uri_schemes_inval_inval_chars, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_not_registered, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_null_bearer, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_null_uri_schemes, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
	cmocka_unit_test_setup_teardown(ccp_call_control_server_test_suite__test_bt_ccp_call_control_server_get_bearer_uri_schemes_inval_zero_size, bt_test_ccp_call_control_server_test_suite_before, bt_test_ccp_call_control_server_test_suite_after),
};
BT_TEST_SUITE(ccp_call_control_server_test_suite, test_cases_ccp_call_control_server_test_suite, bt_test_ccp_call_control_server_test_suite_setup, bt_test_ccp_call_control_server_test_suite_teardown);
