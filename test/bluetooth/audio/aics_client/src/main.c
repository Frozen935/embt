#include "support/test_support.h"

/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/att.h>
#include <embt/bluetooth/audio/aics.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/uuid.h>
#include <fff.h>
#include <embt/base/util.h>
#include "conn.h"
#include "audio/aics_internal.h"

int bt_audio_aics_client_callbacks_init(void);

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, bt_gatt_discover, struct bt_conn *, struct bt_gatt_discover_params *);
FAKE_VALUE_FUNC(int, bt_gatt_subscribe, struct bt_conn *, struct bt_gatt_subscribe_params *);
FAKE_VALUE_FUNC(int, bt_gatt_read, struct bt_conn *, struct bt_gatt_read_params *);
FAKE_VALUE_FUNC(int, bt_gatt_write, struct bt_conn *, struct bt_gatt_write_params *);
FAKE_VALUE_FUNC(int, bt_gatt_write_without_response_cb, struct bt_conn *, uint16_t, const void *,
		uint16_t, bool, bt_gatt_complete_func_t, void *);

FAKE_VOID_FUNC(discover_cb, struct bt_aics *, int);
FAKE_VOID_FUNC(state_cb, struct bt_aics *, int, int8_t, uint8_t, uint8_t);
FAKE_VOID_FUNC(gain_setting_cb, struct bt_aics *, int, uint8_t, int8_t, int8_t);
FAKE_VOID_FUNC(type_cb, struct bt_aics *, int, uint8_t);
FAKE_VOID_FUNC(status_cb, struct bt_aics *, int, bool);
FAKE_VOID_FUNC(mute_cb, struct bt_aics *, int);

#define FFF_FAKES_LIST(FAKE)                                                                       \
	FAKE(bt_gatt_discover)                                                                     \
	FAKE(bt_gatt_subscribe)                                                                    \
	FAKE(bt_gatt_read)                                                                         \
	FAKE(bt_gatt_write)                                                                        \
	FAKE(bt_gatt_write_without_response_cb)                                                    \
	FAKE(discover_cb)                                                                          \
	FAKE(state_cb)                                                                             \
	FAKE(gain_setting_cb)                                                                      \
	FAKE(type_cb)                                                                              \
	FAKE(status_cb)                                                                            \
	FAKE(mute_cb)

struct aics_client_test_suite_fixture {
	struct bt_conn conn;
	struct bt_aics *inst;
};

static void aics_client_discover(struct aics_client_test_suite_fixture *fixture)
{
	const struct bt_gatt_chrc chrcs[] = {
		BT_GATT_CHRC_INIT(BT_UUID_AICS_STATE, 0x0003U,
				  BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY),
		BT_GATT_CHRC_INIT(BT_UUID_AICS_GAIN_SETTINGS, 0x0006U, BT_GATT_CHRC_READ),
		BT_GATT_CHRC_INIT(BT_UUID_AICS_INPUT_TYPE, 0x0008U, BT_GATT_CHRC_READ),
		BT_GATT_CHRC_INIT(BT_UUID_AICS_INPUT_STATUS, 0x000AU,
				  BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY),
		BT_GATT_CHRC_INIT(BT_UUID_AICS_CONTROL, 0x000DU, BT_GATT_CHRC_WRITE),
		BT_GATT_CHRC_INIT(BT_UUID_AICS_DESCRIPTION, 0x000FU,
				  BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP),
		/* Not an AICS characteristic, so the client ignores it */
		BT_GATT_CHRC_INIT(BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678,
									 0x1234, 0x56789abcdef0)),
				  0x0011U, BT_GATT_CHRC_READ),
	};
	const struct bt_aics_discover_param param = {
		.start_handle = 0x0001U,
		.end_handle = 0x0011U,
	};
	struct bt_gatt_discover_params *params;
	int err;

	err = bt_aics_discover(&fixture->conn, fixture->inst, &param);
	assert_int_equal(err, 0);
	assert_int_equal(bt_gatt_discover_fake.call_count, 1U);

	params = bt_gatt_discover_fake.arg1_val;

	ARRAY_FOR_EACH_PTR(chrcs, chrc) {
		struct bt_gatt_chrc value = *chrc;
		const struct bt_gatt_attr attr = {
			.uuid = BT_UUID_GATT_CHRC,
			.handle = chrc->value_handle - 1U,
			.user_data = &value,
		};

		assert_int_equal(params->func(&fixture->conn, &attr, params), BT_GATT_ITER_CONTINUE);
	}

	assert_int_equal(params->func(&fixture->conn, NULL, params), BT_GATT_ITER_STOP);
	assert_int_equal(discover_cb_fake.call_count, 1U);
	assert_int_equal(discover_cb_fake.arg1_val, 0);
}

static void *aics_client_test_suite_setup(void)
{
	assert_int_equal(bt_audio_aics_client_callbacks_init(), 0);
	static struct bt_aics_cb aics_cb = {
		.discover = discover_cb,
		.state = state_cb,
		.gain_setting = gain_setting_cb,
		.type = type_cb,
		.status = status_cb,
		.mute = mute_cb,
	};
	static struct aics_client_test_suite_fixture fixture;

	fixture.inst = bt_aics_client_free_instance_get();
	assert_non_null(fixture.inst);

	bt_aics_client_cb_register(fixture.inst, &aics_cb);

	return &fixture;
}

static void aics_client_test_suite_before(void *f)
{
	struct aics_client_test_suite_fixture *fixture = f;

	FFF_FAKES_LIST(RESET_FAKE);
	FFF_RESET_HISTORY();

	(void)memset(&fixture->conn, 0, sizeof(fixture->conn));
	fixture->conn.info.type = BT_CONN_TYPE_LE;
	fixture->conn.info.state = BT_CONN_STATE_CONNECTED;
	aics_client_discover(fixture);
}

static void aics_client_test_suite_after(void *f)
{
	struct aics_client_test_suite_fixture *fixture = f;

	mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	assert_null(fixture->inst->cli.conn);
}



static void read_rsp(struct aics_client_test_suite_fixture *fixture, const void *data,
		     uint16_t length)
{
	struct bt_gatt_read_params *params = bt_gatt_read_fake.arg1_val;

	assert_non_null(params);
	assert_int_equal(params->func(&fixture->conn, 0U, params, data, length), BT_GATT_ITER_STOP);
}

static void expect_state(int err, int8_t gain, uint8_t mute, uint8_t mode)
{
	assert_int_equal(state_cb_fake.call_count, 1U);
	assert_int_equal(state_cb_fake.arg1_val, err);
	assert_int_equal(state_cb_fake.arg2_val, gain);
	assert_int_equal(state_cb_fake.arg3_val, mute);
	assert_int_equal(state_cb_fake.arg4_val, mode);
}

static void expect_gain_setting(int err, uint8_t units, int8_t minimum, int8_t maximum)
{
	assert_int_equal(gain_setting_cb_fake.call_count, 1U);
	assert_int_equal(gain_setting_cb_fake.arg1_val, err);
	assert_int_equal(gain_setting_cb_fake.arg2_val, units);
	assert_int_equal(gain_setting_cb_fake.arg3_val, minimum);
	assert_int_equal(gain_setting_cb_fake.arg4_val, maximum);
}

static void aics_client_test_suite__test_read_state(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	/* Gain -5, muted, manual mode, change counter 3 */
	const uint8_t value[] = {0xFBU, BT_AICS_STATE_MUTED, BT_AICS_MODE_MANUAL, 0x03U};
	int err;

	err = bt_aics_state_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, value, sizeof(value));
	expect_state(0, -5, BT_AICS_STATE_MUTED, BT_AICS_MODE_MANUAL);
}

static void aics_client_test_suite__test_read_state_empty(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_aics_state_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, NULL, 0U);
	expect_state(BT_ATT_ERR_UNLIKELY, 0, 0U, 0U);
}

static void aics_client_test_suite__test_read_state_short(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	/* Only the first octet is received */
	const uint8_t value[] = {0xFBU, BT_AICS_STATE_MUTED, BT_AICS_MODE_MANUAL, 0x03U};
	int err;

	err = bt_aics_state_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, value, 1U);
	expect_state(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN, 0, 0U, 0U);
}

static void aics_client_test_suite__test_read_gain_setting(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	/* Units 2, minimum -10, maximum 20 */
	const uint8_t value[] = {0x02U, 0xF6U, 0x14U};
	int err;

	err = bt_aics_gain_setting_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, value, sizeof(value));
	expect_gain_setting(0, 2U, -10, 20);
}

static void aics_client_test_suite__test_read_gain_setting_empty(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_aics_gain_setting_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, NULL, 0U);
	expect_gain_setting(BT_ATT_ERR_UNLIKELY, 0U, 0, 0);
}

static void aics_client_test_suite__test_read_gain_setting_short(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	/* Only the first octet is received */
	const uint8_t value[] = {0x02U, 0xF6U, 0x14U};
	int err;

	err = bt_aics_gain_setting_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, value, 1U);
	expect_gain_setting(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN, 0U, 0, 0);
}

static void aics_client_test_suite__test_read_type_empty(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_aics_type_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, NULL, 0U);

	assert_int_equal(type_cb_fake.call_count, 1U);
	assert_int_equal(type_cb_fake.arg1_val, BT_ATT_ERR_UNLIKELY);
	assert_int_equal(type_cb_fake.arg2_val, 0U);
}

static void aics_client_test_suite__test_read_status_empty(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	int err;

	err = bt_aics_status_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, NULL, 0U);

	assert_int_equal(status_cb_fake.call_count, 1U);
	assert_int_equal(status_cb_fake.arg1_val, BT_ATT_ERR_UNLIKELY);
	assert_false(status_cb_fake.arg2_val);
}

static void aics_client_test_suite__test_mute_counter_retry_state_empty(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	/* Gain 0, unmuted, manual mode, change counter 4 */
	const uint8_t state[] = {0x00U, BT_AICS_STATE_UNMUTED, BT_AICS_MODE_MANUAL, 0x04U};
	struct bt_gatt_write_params *write_params;
	int err;

	err = bt_aics_mute(fixture->inst);
	assert_int_equal(err, 0);
	assert_int_equal(bt_gatt_write_fake.call_count, 1U);

	/* A rejected change counter makes the client read the state before writing again */
	write_params = bt_gatt_write_fake.arg1_val;
	write_params->func(&fixture->conn, BT_AICS_ERR_INVALID_COUNTER, write_params);
	assert_int_equal(bt_gatt_read_fake.call_count, 1U);

	read_rsp(fixture, NULL, 0U);

	assert_int_equal(bt_gatt_write_fake.call_count, 1U);
	assert_int_equal(mute_cb_fake.call_count, 1U);
	assert_int_equal(mute_cb_fake.arg1_val, BT_ATT_ERR_UNLIKELY);

	err = bt_aics_mute(fixture->inst);
	assert_int_equal(err, 0);
	assert_int_equal(bt_gatt_write_fake.call_count, 2U);

	/* The failed retry does not keep the next operation from retrying */
	write_params = bt_gatt_write_fake.arg1_val;
	write_params->func(&fixture->conn, BT_AICS_ERR_INVALID_COUNTER, write_params);
	assert_int_equal(bt_gatt_read_fake.call_count, 2U);

	read_rsp(fixture, state, sizeof(state));
	assert_int_equal(bt_gatt_write_fake.call_count, 3U);

	write_params = bt_gatt_write_fake.arg1_val;
	write_params->func(&fixture->conn, 0U, write_params);
	assert_int_equal(mute_cb_fake.call_count, 2U);
	assert_int_equal(mute_cb_fake.arg1_val, 0);
}

static void aics_client_test_suite__test_description_set(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	/* Gain -5, muted, manual mode, change counter 3 */
	const uint8_t value[] = {0xFBU, BT_AICS_STATE_MUTED, BT_AICS_MODE_MANUAL, 0x03U};
	int err;

	err = bt_aics_description_set(fixture->inst, "Mic");
	assert_int_equal(err, 0);
	assert_int_equal(bt_gatt_write_without_response_cb_fake.call_count, 1U);

	/* The write has no completion, so it does not leave the instance busy */
	err = bt_aics_state_get(fixture->inst);
	assert_int_equal(err, 0);

	read_rsp(fixture, value, sizeof(value));
	expect_state(0, -5, BT_AICS_STATE_MUTED, BT_AICS_MODE_MANUAL);
}

static void aics_client_test_suite__test_discover_subscribe_fail(void **cm_state)
{
	struct aics_client_test_suite_fixture *fixture = *cm_state;
	struct bt_gatt_chrc chrc = BT_GATT_CHRC_INIT(BT_UUID_AICS_STATE, 0x0003U,
						     BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY);
	const struct bt_gatt_attr attr = {
		.uuid = BT_UUID_GATT_CHRC,
		.handle = 0x0002U,
		.user_data = &chrc,
	};
	const struct bt_aics_discover_param param = {
		.start_handle = 0x0001U,
		.end_handle = 0x0011U,
	};
	struct bt_gatt_discover_params *params;
	int err;

	err = bt_aics_discover(&fixture->conn, fixture->inst, &param);
	assert_int_equal(err, 0);
	assert_int_equal(bt_gatt_discover_fake.call_count, 2U);

	params = bt_gatt_discover_fake.arg1_val;
	bt_gatt_subscribe_fake.return_val = -ENOMEM;

	assert_int_equal(params->func(&fixture->conn, &attr, params), BT_GATT_ITER_STOP);
	assert_int_equal(discover_cb_fake.call_count, 2U);
	assert_int_equal(discover_cb_fake.arg1_val, -ENOMEM);

	/* The failed discovery does not leave the instance busy */
	RESET_FAKE(bt_gatt_discover);
	RESET_FAKE(bt_gatt_subscribe);
	RESET_FAKE(discover_cb);
	aics_client_discover(fixture);
}

static int bt_test_aics_client_test_suite_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = aics_client_test_suite_setup();
	return 0;
}
static int bt_test_aics_client_test_suite_before(void **state)
{
	(void)state;
	aics_client_test_suite_before(*state);
	return 0;
}
static int bt_test_aics_client_test_suite_after(void **state)
{
	(void)state;
	aics_client_test_suite_after(*state);
	return 0;
}

static const struct CMUnitTest test_cases_aics_client_test_suite[] = {
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_state, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_state_empty, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_state_short, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_gain_setting, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_gain_setting_empty, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_gain_setting_short, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_type_empty, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_read_status_empty, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_mute_counter_retry_state_empty, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_description_set, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
	cmocka_unit_test_setup_teardown(aics_client_test_suite__test_discover_subscribe_fail, bt_test_aics_client_test_suite_before, bt_test_aics_client_test_suite_after),
};
BT_TEST_SUITE(aics_client_test_suite, test_cases_aics_client_test_suite, bt_test_aics_client_test_suite_setup, NULL);
