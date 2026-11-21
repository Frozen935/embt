#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_26f0f3(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_26f0f3(const struct bt_test_case_info *, void *);


/* test_ase_control_params.c - ASE Control Operations with invalid arguments */

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

#include <embt/bluetooth/audio/ascs.h>
#include <embt/bluetooth/gap.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/bluetooth/iso.h>
#include <embt/bluetooth/uuid.h>

#include <embt/compiler.h>
#include <stddef.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/gatt.h>
#include <embt/base/util_macro.h>
#include <sys/types.h>

#include "ascs.h"
#include "bap_stream.h"
#include "conn.h"
#include "gatt_expects.h"
#include "iso.h"

#include "test_common.h"

struct test_ase_control_params_fixture {
	struct bt_conn conn;
	struct bt_bap_stream stream;
	const struct bt_gatt_attr *ase_cp;
	const struct bt_gatt_attr *ase;
};

static void *test_ase_control_params_setup(void)
{
	struct test_ase_control_params_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void test_ase_control_params_before(void *f)
{
	struct test_ase_control_params_fixture *fixture = f;
	struct bt_ascs_register_param param = {
		.snk_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT,
		.src_cnt = CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT,
		.cb = &mock_ascs_cb,
	};
	int err;

	ARG_UNUSED(fixture);

	err = bt_ascs_register(&param);
	assert_int_equal(err, 0);

	test_conn_init(&fixture->conn);
	fixture->ase_cp = test_ase_control_point_get();

	if (IS_ENABLED(CONFIG_BT_ASCS_ASE_SNK)) {
		test_ase_snk_get(CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT, &fixture->ase);
	} else {
		test_ase_src_get(CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT, &fixture->ase);
	}

}

static void test_ase_control_params_after(void *f)
{
	struct test_ase_control_params_fixture *fixture =
		(struct test_ase_control_params_fixture *)f;
	int err;

	if (fixture->conn.info.state == BT_CONN_STATE_CONNECTED) {
		mock_bt_conn_disconnected(&fixture->conn, BT_HCI_ERR_LOCALHOST_TERM_CONN);
	}

	err = bt_ascs_unregister();
	assert_int_equal(err, 0);
}

static void test_ase_control_params_teardown(void *f)
{
	struct test_ase_control_params_fixture *fixture = f;

	free(fixture);
}



static void test_ase_control_params__test_sink_ase_control_operation_zero_length_write(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	uint8_t buf[] = {};
	ssize_t ret;

	ret = fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, (void *)buf, 0, 0, 0);
	assert_true(ret < 0);
}

static void test_expect_unsupported_opcode(struct test_ase_control_params_fixture *fixture,
					   uint8_t opcode)
{
	const uint8_t buf[] = {
		opcode, /* Opcode */
		0x01U,  /* Number_of_ASEs */
		0x01U,  /* ASE_ID[0] */
	};
	const uint8_t data_expected[] = {
		opcode, /* Opcode */
		0xFFU,  /* Number_of_ASEs */
		0x00U,  /* ASE_ID[0] */
		0x01U,  /* Response_Code[0] = Unsupported Opcode */
		0x00U,  /* Reason[0] */
	};

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, (void *)buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_ase_control_params__test_unsupported_opcode_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	test_expect_unsupported_opcode(fixture, 0x00);
}

static void test_ase_control_params__test_unsupported_opcode_rfu(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	test_expect_unsupported_opcode(fixture, 0x09);
}

static void test_codec_configure_expect_invalid_length(
	struct test_ase_control_params_fixture *fixture, const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x01U,          /* Opcode = Config Codec */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

/*
 *  Test correctly formatted ASE Control Point 'Invalid Length' notification is sent
 *
 *  ASCS_v1.0; 5 ASE Control operations
 *  "A client-initiated ASE Control operation shall be defined as an invalid length operation
 *   if the Number_of_ASEs parameter value is less than 1"
 *
 *  Constraints:
 *   - Number_of_ASEs is set to 0
 *   - Config Codec operation parameter array is valid
 *
 *  Expected behaviour:
 *   - "If the Response_Code value is 0x01 or 0x02, Number_of_ASEs shall be set to 0xFF."
 *   - ASE Control Point notification is correctly formatted
 */
static void test_ase_control_params__test_codec_configure_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x00U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[0] */
	};

	test_codec_configure_expect_invalid_length(fixture, buf, sizeof(buf));
}

static void test_ase_control_params__test_codec_configure_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				 CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;

	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	const uint8_t buf[] = {
		0x01U,            /* Opcode = Config Codec */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		0x01U,            /* ASE_ID[0] */
		0x01U,            /* Target_Latency[0] = Target low latency */
		0x02U,            /* Target_PHY[0] = LE 2M PHY */
		0x06U,            /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,     /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,     /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,            /* Codec_Specific_Configuration_Length[0] */
	};

	test_codec_configure_expect_invalid_length(fixture, buf, sizeof(buf));
}

/*
 *  Test correctly formatted ASE Control Point 'Invalid Length' notification is sent
 *
 *  ASCS_v1.0; 5 ASE Control operations
 *  "A client-initiated ASE Control operation shall be defined as an invalid length operation(...)
 *   if the Number_of_ASEs parameter value does not match the number of parameter arrays written by
 *   the client"
 *
 *  Constraints:
 *   - Number_of_ASEs is set to 1
 *   - Config Codec operation parameter arrays != Number_of_ASEs and is set to 2
 *
 *  Expected behaviour:
 *   - "If the Response_Code value is 0x01 or 0x02, Number_of_ASEs shall be set to 0xFF."
 *   - ASE Control Point notification is correctly formatted
 */
static void test_ase_control_params__test_codec_configure_too_many_parameter_arrays(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[0] */
		0x02U,          /* ASE_ID[1] */
		0x01U,          /* Target_Latency[1] = Target low latency */
		0x02U,          /* Target_PHY[1] = LE 2M PHY */
		0x06U,          /* Codec_ID[1].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[1].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[1].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[1] */
	};

	test_codec_configure_expect_invalid_length(fixture, buf, sizeof(buf));
}

/*
 *  Test correctly formatted ASE Control Point 'Invalid Length' notification is sent
 *
 *  ASCS_v1.0; 5 ASE Control operations
 *  "A client-initiated ASE Control operation shall be defined as an invalid length operation
 *   if the total length of all parameters written by the client is not equal to the total length
 *   of all fixed parameters plus the length of any variable length parameters for that operation"
 *
 *  Constraints:
 *   - Number_of_ASEs is set to 1
 *   - Config Codec operation parameter arrays == Number_of_ASEs
 *   - Codec_Specific_Configuration_Length[i] > sizeof(Codec_Specific_Configuration[i])
 *
 *  Expected behaviour:
 *   - "If the Response_Code value is 0x01 or 0x02, Number_of_ASEs shall be set to 0xFF."
 *   - ASE Control Point notification is correctly formatted
 */
static void test_ase_control_params__test_codec_specific_configuration_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x05U,          /* Codec_Specific_Configuration_Length[0] */
		0x00U, 0x00U,   /* Codec_Specific_Configuration[0] */
		0x00U, 0x00U,
	};

	test_codec_configure_expect_invalid_length(fixture, buf, sizeof(buf));
}

/*
 *  Test correctly formatted ASE Control Point 'Invalid Length' notification is sent
 *
 *  ASCS_v1.0; 5 ASE Control operations
 *  "A client-initiated ASE Control operation shall be defined as an invalid length operation
 *   if the total length of all parameters written by the client is not equal to the total length
 *   of all fixed parameters plus the length of any variable length parameters for that operation"
 *
 *  Constraints:
 *   - Number_of_ASEs is set to 1
 *   - Config Codec operation parameter arrays == Number_of_ASEs
 *   - Codec_Specific_Configuration_Length[i] < sizeof(Codec_Specific_Configuration[i])
 *
 *  Expected behaviour:
 *   - "If the Response_Code value is 0x01 or 0x02, Number_of_ASEs shall be set to 0xFF."
 *   - ASE Control Point notification is correctly formatted
 */
static void test_ase_control_params__test_codec_specific_configuration_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x05U,          /* Codec_Specific_Configuration_Length[0] */
		0x00U, 0x00U,   /* Codec_Specific_Configuration[0] */
		0x00U, 0x00U,
		0x00U, 0x00U,
	};

	test_codec_configure_expect_invalid_length(fixture, buf, sizeof(buf));
}

/*
 *  Test correctly formatted ASE Control Point 'Invalid ASE_ID' notification is sent
 *
 *  Constraints:
 *   - Number_of_ASEs is set to 1
 *   - Requested ASE_ID is not present on the server.
 *
 *  Expected behaviour:
 *   - Correctly formatted ASE Control Point notification is sent with Invalid ASE_ID response code.
 */
static void test_ase_control_params__test_codec_configure_invalid_ase_id_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id_invalid = 0x00U;
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		ase_id_invalid, /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[0] */
	};
	const uint8_t data_expected[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		ase_id_invalid, /* ASE_ID[0] */
		0x03U,          /* Response_Code[0] = Invalid ASE_ID */
		0x00U,          /* Reason[0] */
	};

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static struct bt_bap_stream test_stream;
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

	*stream = &test_stream;
	*pref = qos_pref;
	*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_SUCCESS, BT_BAP_ASCS_REASON_NONE);

	bt_bap_stream_cb_register(*stream, &mock_bap_stream_ops);

	return 0;
}

static void test_ase_control_params__test_codec_configure_invalid_ase_id_unavailable(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	/* Test requires support for at least 2 ASEs */
	if (CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT + CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT < 2) {
		skip();
	}

	const uint8_t ase_id_valid = 0x01U;
	const uint8_t ase_id_invalid = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				       CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x02U,          /* Number_of_ASEs */
		ase_id_invalid, /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] = Target low latency */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x04U,          /* Codec_Specific_Configuration_Length[0] */
		0x00U, 0x00U,   /* Codec_Specific_Configuration[0] */
		0x00U, 0x00U,
		ase_id_valid,   /* ASE_ID[1] */
		0x01U,          /* Target_Latency[1] = Target low latency */
		0x02U,          /* Target_PHY[1] = LE 2M PHY */
		0x06U,          /* Codec_ID[1].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[1].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[1].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[1] */
	};
	const uint8_t data_expected[] = {
		0x01U,          /* Opcode = Config Codec */
		0x02U,          /* Number_of_ASEs */
		ase_id_invalid, /* ASE_ID[0] */
		0x03U,          /* Response_Code[0] = Invalid ASE_ID */
		0x00U,          /* Reason[0] */
		ase_id_valid,   /* ASE_ID[1] */
		0x00U,          /* Response_Code[1] = Success */
		0x00U,          /* Reason[1] */
	};

	mock_ascs_cb_config_fake.custom_fake = ascs_cb_config_custom_fake;

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_target_latency_out_of_range(struct test_ase_control_params_fixture *fixture,
					     uint8_t target_latency)
{
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		target_latency, /* Target_Latency[0] */
		0x02U,          /* Target_PHY[0] = LE 2M PHY */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[0] */
	};
	const uint8_t data_expected[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x09U,          /* Response_Code[0] = Invalid codec configuration */
		0x08U,          /* Reason[0] = Reason Latency */
	};

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_ase_control_params__test_target_latency_out_of_range_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	test_target_latency_out_of_range(fixture, 0x00);
}

static void test_ase_control_params__test_target_latency_out_of_range_0x04(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	test_target_latency_out_of_range(fixture, 0x04);
}

static void test_target_phy_out_of_range(struct test_ase_control_params_fixture *fixture,
					 uint8_t target_phy)
{
	const uint8_t buf[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x01U,          /* Target_Latency[0] */
		target_phy,     /* Target_PHY[0] */
		0x06U,          /* Codec_ID[0].Coding_Format = LC3 */
		0x00U, 0x00U,   /* Codec_ID[0].Company_ID */
		0x00U, 0x00U,   /* Codec_ID[0].Vendor_Specific_Codec_ID */
		0x00U,          /* Codec_Specific_Configuration_Length[0] */
	};
	const uint8_t data_expected[] = {
		0x01U,          /* Opcode = Config Codec */
		0x01U,          /* Number_of_ASEs */
		0x01U,          /* ASE_ID[0] */
		0x09U,          /* Response_Code[0] = Invalid codec configuration */
		0x05U,          /* Reason[0] = Reason PHY */
	};

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_ase_control_params__test_target_phy_out_of_range_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	test_target_phy_out_of_range(fixture, 0x00);
}

static void test_ase_control_params__test_target_phy_out_of_range_0x04(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	test_target_phy_out_of_range(fixture, 0x04);
}

static void test_config_qos_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
						  const struct bt_gatt_attr *ase_cp,
						  struct bt_bap_stream *stream,
						  const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x02U,          /* Opcode = Config QoS */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};

	test_preamble_state_codec_configured(conn, ase_id, stream);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_config_qos_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x02U,                  /* Opcode = Config QoS */
		0x00U,                  /* Number_of_ASEs */
		ase_id,                 /* ASE_ID[0] */
		0x01U,                  /* CIG_ID[0] */
		0x01U,                  /* CIS_ID[0] */
		0xFFU, 0x00U, 0x00U,    /* SDU_Interval[0] */
		0x00U,                  /* Framing[0] */
		0x02U,                  /* PHY[0] */
		0x64U, 0x00U,           /* Max_SDU[0] */
		0x02U,                  /* Retransmission_Number[0] */
		0x0AU, 0x00U,           /* Max_Transport_Latency[0] */
		0x40U, 0x9CU, 0x00U,    /* Presentation_Delay[0] */
	};

	test_config_qos_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					      &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_config_qos_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				 CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;

	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x02U,            /* Opcode = Config QoS */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
		0x01U,            /* CIG_ID[0] */
		0x01U,            /* CIS_ID[0] */
		0xFFU, 0x00U, 0x00U, /* SDU_Interval[0] */
		0x00U,            /* Framing[0] */
		0x02U,            /* PHY[0] */
		0x64U, 0x00U,     /* Max_SDU[0] */
		0x02U,            /* Retransmission_Number[0] */
		0x0AU, 0x00U,     /* Max_Transport_Latency[0] */
		0x40U, 0x9CU, 0x00U, /* Presentation_Delay[0] */
	};

	test_config_qos_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					      &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_config_qos_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x02U,                  /* Opcode = Config QoS */
		0x01U,                  /* Number_of_ASEs */
		ase_id,                 /* ASE_ID[0] */
		0x01U,                  /* CIG_ID[0] */
		0x01U,                  /* CIS_ID[0] */
		0xFFU, 0x00U, 0x00U,    /* SDU_Interval[0] */
		0x00U,                  /* Framing[0] */
		0x02U,                  /* PHY[0] */
		0x64U, 0x00U,           /* Max_SDU[0] */
		0x02U,                  /* Retransmission_Number[0] */
		0x0AU, 0x00U,           /* Max_Transport_Latency[0] */
		0x40U, 0x9CU, 0x00U,    /* Presentation_Delay[0] */
		0x00U,
	};

	test_config_qos_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					      &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_config_qos_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x02U,                  /* Opcode = Config QoS */
		0x01U,                  /* Number_of_ASEs */
		ase_id,                 /* ASE_ID[0] */
		0x01U,                  /* CIG_ID[0] */
		0x01U,                  /* CIS_ID[0] */
		0xFFU, 0x00U, 0x00U,    /* SDU_Interval[0] */
		0x00U,                  /* Framing[0] */
		0x02U,                  /* PHY[0] */
		0x64U, 0x00U,           /* Max_SDU[0] */
		0x02U,                  /* Retransmission_Number[0] */
		0x0AU, 0x00U,           /* Max_Transport_Latency[0] */
		0x40U, 0x9CU,           /* Presentation_Delay[0] */
	};

	test_config_qos_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					      &fixture->stream, buf, sizeof(buf));
}

static void test_enable_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
					      const struct bt_gatt_attr *ase_cp,
					      struct bt_bap_stream *stream,
					      const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x03U,          /* Opcode = Enable */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};

	test_preamble_state_qos_configured(conn, ase_id, stream);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_enable_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x03U,          /* Opcode = Enable */
		0x00U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,          /* Metadata_Length[0] */
	};

	test_enable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp, &fixture->stream,
					  buf, sizeof(buf));
}

static void test_ase_control_params__test_enable_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				 CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;

	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x03U,            /* Opcode = Enable */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
		0x00U,            /* Metadata_Length[0] */
	};

	test_enable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp, &fixture->stream,
					  buf, sizeof(buf));
}

static void test_ase_control_params__test_enable_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x03U,          /* Opcode = Enable */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,          /* Metadata_Length[0] */
		0x00U,
	};

	test_enable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp, &fixture->stream,
					  buf, sizeof(buf));
}

static void test_ase_control_params__test_enable_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x03U,          /* Opcode = Enable */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
	};

	test_enable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp, &fixture->stream,
					  buf, sizeof(buf));
}

static void test_ase_control_params__test_enable_metadata_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x03U,          /* Opcode = Enable */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x03U,          /* Metadata_Length[0] */
		0x02U, 0x02U,   /* Metadata[0] */
	};

	test_enable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp, &fixture->stream,
					  buf, sizeof(buf));
}

static void test_ase_control_params__test_enable_invalid_ase_id(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	/* Test requires support for at least 2 ASEs */
	if (CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT + CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT < 2) {
		skip();
	}

	const uint8_t ase_id_valid = 0x01U;
	const uint8_t ase_id_invalid = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				       CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;
	const uint8_t buf[] = {
		0x03U,                      /* Opcode = Enable */
		0x02U,                      /* Number_of_ASEs */
		ase_id_invalid,             /* ASE_ID[0] */
		0x04U,                      /* Metadata_Length[0] */
		0x03U, 0x02U, 0x04U, 0x00U, /* Metadata[0] = Streaming Context (Media) */
		ase_id_valid,               /* ASE_ID[1] */
		0x04U,                      /* Metadata_Length[0] */
		0x03U, 0x02U, 0x04U, 0x00U, /* Metadata[0] = Streaming Context (Media) */
	};
	const uint8_t data_expected[] = {
		0x03U,                  /* Opcode = Enable */
		0x02U,                  /* Number_of_ASEs */
		ase_id_invalid,         /* ASE_ID[0] */
		0x03U,                  /* Response_Code[0] = Invalid ASE_ID */
		0x00U,                  /* Reason[0] */
		ase_id_valid,           /* ASE_ID[1] */
		0x00U,                  /* Response_Code[1] = Success */
		0x00U,                  /* Reason[1] */
	};

	test_preamble_state_qos_configured(&fixture->conn, ase_id_valid, &fixture->stream);

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_ase_control_params__test_enable_metadata_prohibited_context(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id_valid = 0x01U;
	const uint8_t buf[] = {
		0x03U,                      /* Opcode = Enable */
		0x01U,                      /* Number_of_ASEs */
		ase_id_valid,               /* ASE_ID[0] */
		0x04U,                      /* Metadata_Length[0] */
		0x03U, 0x02U, 0x00U, 0x00U, /* Metadata[0] = Streaming Context (Prohibited) */
	};
	const uint8_t data_expected[] = {
		0x03U,                  /* Opcode = Enable */
		0x01U,                  /* Number_of_ASEs */
		ase_id_valid,           /* ASE_ID[0] */
		0x0CU,                  /* Response_Code[0] = Invalid Metadata */
		0x02U,                  /* Reason[0] = Streaming Context */
	};

	test_preamble_state_qos_configured(&fixture->conn, ase_id_valid, &fixture->stream);

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_receiver_start_ready_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
							    const struct bt_gatt_attr *ase_cp,
							    struct bt_bap_stream *stream,
							    const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x04U,          /* Opcode = Receiver Start Ready */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};
	struct bt_iso_chan *chan;
	int err;

	test_preamble_state_enabling(conn, ase_id, stream);

	err = mock_bt_iso_accept(conn, 0x01, 0x01, &chan);
	assert_int_equal(0, err);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_receiver_start_ready_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase;
	uint8_t ase_id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	ase_id = test_ase_id_get(ase);

	const uint8_t buf[] = {
		0x04U,          /* Opcode = Receiver Start Ready */
		0x00U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
	};

	test_receiver_start_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
							&fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_receiver_start_ready_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;
	const struct bt_gatt_attr *ase;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	const uint8_t ase_id = test_ase_id_get(ase);

	const uint8_t buf[] = {
		0x04U,            /* Opcode = Receiver Start Ready */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
	};

	test_receiver_start_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
							&fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_receiver_start_ready_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase;
	uint8_t ase_id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	ase_id = test_ase_id_get(fixture->ase);

	const uint8_t buf[] = {
		0x04U,          /* Opcode = Receiver Start Ready */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,
	};

	test_receiver_start_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
							&fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_receiver_start_ready_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t buf[] = {
		0x04U,          /* Opcode = Receiver Start Ready */
		0x01U,          /* Number_of_ASEs */
	};
	const struct bt_gatt_attr *ase;
	uint8_t ase_id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	ase_id = test_ase_id_get(fixture->ase);

	test_receiver_start_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
							&fixture->stream, buf, sizeof(buf));
}

static void test_disable_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
					       const struct bt_gatt_attr *ase_cp,
					       struct bt_bap_stream *stream,
					       const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x05U,          /* Opcode = Disable */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};

	test_preamble_state_enabling(conn, ase_id, stream);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_disable_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x05U,          /* Opcode = Disable */
		0x00U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
	};

	test_disable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_disable_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				 CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;

	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x05U,            /* Opcode = Disable */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
	};

	test_disable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_disable_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x05U,          /* Opcode = Disable */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,
	};

	test_disable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_disable_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x05U,          /* Opcode = Disable */
		0x01U,          /* Number_of_ASEs */
	};

	test_disable_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_receiver_stop_ready_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
							   const struct bt_gatt_attr *ase_cp,
							   struct bt_bap_stream *stream,
							   const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x06U,          /* Opcode = Receiver Stop Ready */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};
	struct bt_iso_chan *chan;

	test_preamble_state_disabling(conn, ase_id, stream, &chan);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_receiver_stop_ready_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase;
	uint8_t ase_id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	ase_id = test_ase_id_get(ase);

	const uint8_t buf[] = {
		0x06U,          /* Opcode = Receiver Stop Ready */
		0x00U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
	};

	test_receiver_stop_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						       &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_receiver_stop_ready_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;
	const struct bt_gatt_attr *ase;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	const uint8_t ase_id = test_ase_id_get(ase);

	const uint8_t buf[] = {
		0x06U,            /* Opcode = Receiver Stop Ready */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
	};

	test_receiver_stop_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						       &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_receiver_stop_ready_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const struct bt_gatt_attr *ase;
	uint8_t ase_id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	ase_id = test_ase_id_get(fixture->ase);

	const uint8_t buf[] = {
		0x06U,          /* Opcode = Receiver Stop Ready */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,
	};

	test_receiver_stop_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						       &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_receiver_stop_ready_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t buf[] = {
		0x06U,          /* Opcode = Receiver Stop Ready */
		0x01U,          /* Number_of_ASEs */
	};
	const struct bt_gatt_attr *ase;
	uint8_t ase_id;

	#if !defined(CONFIG_BT_ASCS_ASE_SRC)
	skip();
#endif


	test_ase_src_get(1, &ase);
	do { if (!(ase)) { skip(); } } while (0);
	ase_id = test_ase_id_get(fixture->ase);

	test_receiver_stop_ready_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						       &fixture->stream, buf, sizeof(buf));
}

static void test_update_metadata_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
						       const struct bt_gatt_attr *ase_cp,
						       struct bt_bap_stream *stream,
						       const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x07U,          /* Opcode = Update Metadata */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};

	test_preamble_state_enabling(conn, ase_id, stream);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_update_metadata_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x07U,          /* Opcode = Update Metadata */
		0x00U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,          /* Metadata_Length[0] */
	};

	test_update_metadata_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_update_metadata_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				 CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;

	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x07U,            /* Opcode = Update Metadata */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
		0x00U,            /* Metadata_Length[0] */
	};

	test_update_metadata_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_update_metadata_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x07U,          /* Opcode = Update Metadata */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,          /* Metadata_Length[0] */
		0x00U,
	};

	test_update_metadata_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_update_metadata_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x07U,          /* Opcode = Update Metadata */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
	};

	test_update_metadata_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_update_metadata_metadata_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x07U,          /* Opcode = Update Metadata */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x03U,          /* Metadata_Length[0] */
		0x02U, 0x02U,   /* Metadata[0] */
	};

	test_update_metadata_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
						   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_update_metadata_invalid_ase_id(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	/* Test requires support for at least 2 ASEs */
	if (CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT + CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT < 2) {
		skip();
	}

	const uint8_t ase_id_valid = 0x01U;
	const uint8_t ase_id_invalid = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				       CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;
	const uint8_t buf[] = {
		0x07U,                      /* Opcode = Update Metadata */
		0x02U,                      /* Number_of_ASEs */
		ase_id_invalid,             /* ASE_ID[0] */
		0x04U,                      /* Metadata_Length[0] */
		0x03U, 0x02U, 0x04U, 0x00U, /* Metadata[0] = Streaming Context (Media) */
		ase_id_valid,               /* ASE_ID[1] */
		0x04U,                      /* Metadata_Length[0] */
		0x03U, 0x02U, 0x04U, 0x00U, /* Metadata[0] = Streaming Context (Media) */
	};
	const uint8_t data_expected[] = {
		0x07U,                  /* Opcode = Update Metadata */
		0x02U,                  /* Number_of_ASEs */
		ase_id_invalid,         /* ASE_ID[0] */
		0x03U,                  /* Response_Code[0] = Invalid ASE_ID */
		0x00U,                  /* Reason[0] */
		ase_id_valid,           /* ASE_ID[1] */
		0x00U,                  /* Response_Code[1] = Success */
		0x00U,                  /* Reason[1] */
	};

	test_preamble_state_enabling(&fixture->conn, ase_id_valid, &fixture->stream);

	fixture->ase_cp->write(&fixture->conn, fixture->ase_cp, buf, sizeof(buf), 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(&fixture->conn, BT_UUID_ASCS_ASE_CP, fixture->ase_cp,
					     data_expected, sizeof(data_expected));
}

static void test_release_expect_invalid_length(struct bt_conn *conn, uint8_t ase_id,
					       const struct bt_gatt_attr *ase_cp,
					       struct bt_bap_stream *stream,
					       const uint8_t *buf, size_t len)
{
	const uint8_t data_expected[] = {
		0x08U,          /* Opcode = Release */
		0xFFU,          /* Number_of_ASEs */
		0x00U,          /* ASE_ID[0] */
		0x02U,          /* Response_Code[0] = Invalid Length */
		0x00U,          /* Reason[0] */
	};

	test_preamble_state_enabling(conn, ase_id, stream);

	ase_cp->write(conn, ase_cp, buf, len, 0, 0);

	test_drain_syswq(); /* Ensure that state transitions are completed */
	expect_bt_gatt_notify_cb_called_with(conn, BT_UUID_ASCS_ASE_CP, ase_cp, data_expected,
					     sizeof(data_expected));
}

static void test_ase_control_params__test_release_number_of_ases_0x00(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x08U,          /* Opcode = Release */
		0x00U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
	};

	test_release_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_release_number_of_ases_above_max(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint16_t ase_cnt = CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT +
				 CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT + 1;

	/* Skip if number of ASEs configured is high enough to support any value in the write req */
	if (ase_cnt > UINT8_MAX) {
		skip();
	}

	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x08U,            /* Opcode = Release */
		(uint8_t)ase_cnt, /* Number_of_ASEs */
		ase_id,           /* ASE_ID[0] */
	};

	test_release_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_release_too_long(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x08U,          /* Opcode = Release */
		0x01U,          /* Number_of_ASEs */
		ase_id,         /* ASE_ID[0] */
		0x00U,
	};

	test_release_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

static void test_ase_control_params__test_release_too_short(void **cm_state)
{
	struct test_ase_control_params_fixture *fixture = *cm_state;
	const uint8_t ase_id = test_ase_id_get(fixture->ase);
	const uint8_t buf[] = {
		0x08U,          /* Opcode = Release */
		0x01U,          /* Number_of_ASEs */
	};

	test_release_expect_invalid_length(&fixture->conn, ase_id, fixture->ase_cp,
					   &fixture->stream, buf, sizeof(buf));
}

int bt_test_test_ase_control_params_setup(void **state)
{
	int environment_result = bt_test_audio_ascs_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = test_ase_control_params_setup();
	return 0;
}
static int bt_test_test_ase_control_params_before(void **state)
{
	(void)state;
	test_ase_control_params_before(*state);
	mock_init_rule_before_26f0f3(NULL, *state);
	return 0;
}
int bt_test_test_ase_control_params_after(void **state)
{
	(void)state;
	test_ase_control_params_after(*state);
	mock_destroy_rule_after_26f0f3(NULL, *state);
	return 0;
}
int bt_test_test_ase_control_params_teardown(void **state)
{
	(void)state;
	test_ase_control_params_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_test_ase_control_params[] = {
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_sink_ase_control_operation_zero_length_write, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_unsupported_opcode_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_unsupported_opcode_rfu, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_configure_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_configure_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_configure_too_many_parameter_arrays, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_specific_configuration_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_specific_configuration_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_configure_invalid_ase_id_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_codec_configure_invalid_ase_id_unavailable, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_target_latency_out_of_range_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_target_latency_out_of_range_0x04, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_target_phy_out_of_range_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_target_phy_out_of_range_0x04, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_config_qos_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_config_qos_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_config_qos_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_config_qos_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_metadata_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_invalid_ase_id, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_enable_metadata_prohibited_context, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_start_ready_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_start_ready_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_start_ready_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_start_ready_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_disable_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_disable_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_disable_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_disable_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_stop_ready_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_stop_ready_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_stop_ready_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_receiver_stop_ready_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_update_metadata_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_update_metadata_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_update_metadata_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_update_metadata_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_update_metadata_metadata_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_update_metadata_invalid_ase_id, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_release_number_of_ases_0x00, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_release_number_of_ases_above_max, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_release_too_long, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
	cmocka_unit_test_setup_teardown(test_ase_control_params__test_release_too_short, bt_test_test_ase_control_params_before, bt_test_test_ase_control_params_after),
};
BT_TEST_SUITE(test_ase_control_params, test_cases_test_ase_control_params, bt_test_test_ase_control_params_setup, bt_test_test_ase_control_params_teardown);
