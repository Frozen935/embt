#include "support/test_support.h"
/* sdp_client.c - Bluetooth classic SDP client smoke test */

/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <embt/base/log.h>
#include <embt/base/byteorder.h>

#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/classic/rfcomm.h>
#include <embt/bluetooth/classic/sdp.h>

#include <shell/bt_shell.h>

extern struct bt_conn *default_conn;

static struct bt_sdp_discover_params sdp_discover;
static union {
	struct bt_uuid_16 u16;
	struct bt_uuid_32 u32;
	struct bt_uuid_128 u128;
} sdp_discover_uuid;

const uint16_t svclass_list[] = {
	BT_SDP_SDP_SERVER_SVCLASS,
	BT_SDP_BROWSE_GRP_DESC_SVCLASS,
	BT_SDP_PUBLIC_BROWSE_GROUP,
	BT_SDP_SERIAL_PORT_SVCLASS,
	BT_SDP_LAN_ACCESS_SVCLASS,
	BT_SDP_DIALUP_NET_SVCLASS,
	BT_SDP_IRMC_SYNC_SVCLASS,
	BT_SDP_OBEX_OBJPUSH_SVCLASS,
	BT_SDP_OBEX_FILETRANS_SVCLASS,
	BT_SDP_IRMC_SYNC_CMD_SVCLASS,
	BT_SDP_HEADSET_SVCLASS,
	BT_SDP_CORDLESS_TELEPHONY_SVCLASS,
	BT_SDP_AUDIO_SOURCE_SVCLASS,
	BT_SDP_AUDIO_SINK_SVCLASS,
	BT_SDP_AV_REMOTE_TARGET_SVCLASS,
	BT_SDP_ADVANCED_AUDIO_SVCLASS,
	BT_SDP_AV_REMOTE_SVCLASS,
	BT_SDP_AV_REMOTE_CONTROLLER_SVCLASS,
	BT_SDP_INTERCOM_SVCLASS,
	BT_SDP_FAX_SVCLASS,
	BT_SDP_HEADSET_AGW_SVCLASS,
	BT_SDP_WAP_SVCLASS,
	BT_SDP_WAP_CLIENT_SVCLASS,
	BT_SDP_PANU_SVCLASS,
	BT_SDP_NAP_SVCLASS,
	BT_SDP_GN_SVCLASS,
	BT_SDP_DIRECT_PRINTING_SVCLASS,
	BT_SDP_REFERENCE_PRINTING_SVCLASS,
	BT_SDP_IMAGING_SVCLASS,
	BT_SDP_IMAGING_RESPONDER_SVCLASS,
	BT_SDP_IMAGING_ARCHIVE_SVCLASS,
	BT_SDP_IMAGING_REFOBJS_SVCLASS,
	BT_SDP_HANDSFREE_SVCLASS,
	BT_SDP_HANDSFREE_AGW_SVCLASS,
	BT_SDP_DIRECT_PRT_REFOBJS_SVCLASS,
	BT_SDP_REFLECTED_UI_SVCLASS,
	BT_SDP_BASIC_PRINTING_SVCLASS,
	BT_SDP_PRINTING_STATUS_SVCLASS,
	BT_SDP_HID_SVCLASS,
	BT_SDP_HCR_SVCLASS,
	BT_SDP_HCR_PRINT_SVCLASS,
	BT_SDP_HCR_SCAN_SVCLASS,
	BT_SDP_CIP_SVCLASS,
	BT_SDP_VIDEO_CONF_GW_SVCLASS,
	BT_SDP_UDI_MT_SVCLASS,
	BT_SDP_UDI_TA_SVCLASS,
	BT_SDP_AV_SVCLASS,
	BT_SDP_SAP_SVCLASS,
	BT_SDP_PBAP_PCE_SVCLASS,
	BT_SDP_PBAP_PSE_SVCLASS,
	BT_SDP_PBAP_SVCLASS,
	BT_SDP_MAP_MSE_SVCLASS,
	BT_SDP_MAP_MCE_SVCLASS,
	BT_SDP_MAP_SVCLASS,
	BT_SDP_GNSS_SVCLASS,
	BT_SDP_GNSS_SERVER_SVCLASS,
	BT_SDP_MPS_SC_SVCLASS,
	BT_SDP_MPS_SVCLASS,
	BT_SDP_PNP_INFO_SVCLASS,
	BT_SDP_GENERIC_NETWORKING_SVCLASS,
	BT_SDP_GENERIC_FILETRANS_SVCLASS,
	BT_SDP_GENERIC_AUDIO_SVCLASS,
	BT_SDP_GENERIC_TELEPHONY_SVCLASS,
	BT_SDP_UPNP_SVCLASS,
	BT_SDP_UPNP_IP_SVCLASS,
	BT_SDP_UPNP_PAN_SVCLASS,
	BT_SDP_UPNP_LAP_SVCLASS,
	BT_SDP_UPNP_L2CAP_SVCLASS,
	BT_SDP_VIDEO_SOURCE_SVCLASS,
	BT_SDP_VIDEO_SINK_SVCLASS,
	BT_SDP_VIDEO_DISTRIBUTION_SVCLASS,
	BT_SDP_HDP_SVCLASS,
	BT_SDP_HDP_SOURCE_SVCLASS,
	BT_SDP_HDP_SINK_SVCLASS,
	BT_SDP_GENERIC_ACCESS_SVCLASS,
	BT_SDP_GENERIC_ATTRIB_SVCLASS,
	BT_SDP_APPLE_AGENT_SVCLASS,
};

#define SDP_CLIENT_USER_BUF_LEN 4096
NET_BUF_POOL_FIXED_DEFINE(sdp_client_pool, 1, SDP_CLIENT_USER_BUF_LEN,
			  CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);

static bool sdp_record_found;

static uint8_t sdp_discover_func(struct bt_conn *conn, struct bt_sdp_client_result *result,
				 const struct bt_sdp_discover_params *params)
{
	int err;
	uint16_t param;

	if ((result == NULL) || (result->resp_buf == NULL) || (result->resp_buf->len == 0)) {
		if (sdp_record_found) {
			sdp_record_found = false;
			bt_log_print("SDP Discovery Done\n");
		} else {
			bt_log_print("No SDP Record\n");
		}
		return BT_SDP_DISCOVER_UUID_STOP;
	}

	sdp_record_found = true;

	bt_log_print("SDP Rsp Data:\n");
	err = bt_sdp_get_proto_param(result->resp_buf, BT_SDP_PROTO_L2CAP, &param);
	if (!err) {
		bt_log_print("    PROTOCOL: L2CAP: %d\n", param);
	}
	err = bt_sdp_get_proto_param(result->resp_buf, BT_SDP_PROTO_RFCOMM, &param);
	if (!err) {
		bt_log_print("    PROTOCOL: RFCOMM: %d\n", param);
	}
	for (size_t i = 0; i < ARRAY_SIZE(svclass_list); i++) {
		err = bt_sdp_get_profile_version(result->resp_buf, svclass_list[i], &param);
		if (!err) {
			bt_log_print("    VERSION: %04X: %d\n", svclass_list[i], param);
		}
	}
	err = bt_sdp_get_features(result->resp_buf, &param);
	if (!err) {
		bt_log_print("    FEATURE: %04X\n", param);
	}
	bt_log_print("    RAW:");
	for (uint16_t i = 0; i < result->resp_buf->len; i++) {
		bt_log_print("%02X", result->resp_buf->data[i]);
	}
	bt_log_print("\n");

	if (!result->next_record_hint) {
		sdp_record_found = false;
		bt_log_print("SDP Discovery Done\n");
	}

	return BT_SDP_DISCOVER_UUID_CONTINUE;
}

static struct bt_sdp_attribute_id_list attr_ids;
static struct bt_sdp_attribute_id_range attr_id_ranges[1];

static int cmd_ssa_discovery(const struct bt_shell *sh, size_t argc, char *argv[])
{
	int err = 0;
	size_t len;
	uint8_t uuid128[BT_UUID_SIZE_128];

	len = strlen(argv[1]);

	if (len == (BT_UUID_SIZE_16 * 2)) {
		uint16_t val;

		sdp_discover_uuid.u16.uuid.type = BT_UUID_TYPE_16;
		hex2bin(argv[1], len, (uint8_t *)&val, sizeof(val));
		sdp_discover_uuid.u16.val = sys_be16_to_cpu(val);
		sdp_discover.uuid = &sdp_discover_uuid.u16.uuid;
	} else if (len == (BT_UUID_SIZE_32 * 2)) {
		uint32_t val;

		sdp_discover_uuid.u32.uuid.type = BT_UUID_TYPE_32;
		hex2bin(argv[1], len, (uint8_t *)&val, sizeof(val));
		sdp_discover_uuid.u32.val = sys_be32_to_cpu(val);
		sdp_discover.uuid = &sdp_discover_uuid.u32.uuid;
	} else if (len == (BT_UUID_SIZE_128 * 2)) {
		sdp_discover_uuid.u128.uuid.type = BT_UUID_TYPE_128;
		hex2bin(argv[1], len, &uuid128[0], sizeof(uuid128));
		sys_memcpy_swap(sdp_discover_uuid.u128.val, uuid128, sizeof(uuid128));
		sdp_discover.uuid = &sdp_discover_uuid.u128.uuid;
	} else {
		bt_shell_session_error(sh, "Invalid UUID");
		return -ENOEXEC;
	}

	attr_ids.count = 0;

	if (argc > 2) {
		attr_ids.count = ARRAY_SIZE(attr_id_ranges);
		attr_ids.ranges = attr_id_ranges;
		attr_id_ranges[0].beginning = (uint16_t)bt_shell_strtol(argv[2], 0, &err);
		if (err < 0) {
			bt_shell_session_error(sh, "Invalid beginning ATTR ID");
			return -ENOEXEC;
		}
		attr_id_ranges[0].ending = 0xffff;
	}

	if (argc > 3) {
		attr_id_ranges[0].ending = (uint16_t)bt_shell_strtol(argv[3], 0, &err);
		if (err < 0) {
			bt_shell_session_error(sh, "Invalid ending ATTR ID");
			return -ENOEXEC;
		}
	}

	sdp_discover.func = sdp_discover_func;
	sdp_discover.pool = &sdp_client_pool;
	sdp_discover.type = BT_SDP_DISCOVER_SERVICE_SEARCH_ATTR;
	if (attr_ids.count != 0) {
		sdp_discover.ids = &attr_ids;
	}

	err = bt_sdp_discover(default_conn, &sdp_discover);
	if (err) {
		bt_shell_session_error(sh, "Fail to start SDP Discovery (err %d)", err);
		return err;
	}
	return 0;
}

static int cmd_ss_discovery(const struct bt_shell *sh, size_t argc, char *argv[])
{
	int err;
	size_t len;
	uint8_t uuid128[BT_UUID_SIZE_128];

	len = strlen(argv[1]);

	if (len == (BT_UUID_SIZE_16 * 2)) {
		uint16_t val;

		sdp_discover_uuid.u16.uuid.type = BT_UUID_TYPE_16;
		hex2bin(argv[1], len, (uint8_t *)&val, sizeof(val));
		sdp_discover_uuid.u16.val = sys_be16_to_cpu(val);
		sdp_discover.uuid = &sdp_discover_uuid.u16.uuid;
	} else if (len == (BT_UUID_SIZE_32 * 2)) {
		uint32_t val;

		sdp_discover_uuid.u32.uuid.type = BT_UUID_TYPE_32;
		hex2bin(argv[1], len, (uint8_t *)&val, sizeof(val));
		sdp_discover_uuid.u32.val = sys_be32_to_cpu(val);
		sdp_discover.uuid = &sdp_discover_uuid.u32.uuid;
	} else if (len == (BT_UUID_SIZE_128 * 2)) {
		sdp_discover_uuid.u128.uuid.type = BT_UUID_TYPE_128;
		hex2bin(argv[1], len, &uuid128[0], sizeof(uuid128));
		sys_memcpy_swap(sdp_discover_uuid.u128.val, uuid128, sizeof(uuid128));
		sdp_discover.uuid = &sdp_discover_uuid.u128.uuid;
	} else {
		bt_shell_session_error(sh, "Invalid UUID");
		return -ENOEXEC;
	}

	sdp_discover.func = sdp_discover_func;
	sdp_discover.pool = &sdp_client_pool;
	sdp_discover.type = BT_SDP_DISCOVER_SERVICE_SEARCH;

	err = bt_sdp_discover(default_conn, &sdp_discover);
	if (err) {
		bt_shell_session_error(sh, "Fail to start SDP Discovery (err %d)", err);
		return err;
	}
	return 0;
}

static int cmd_sa_discovery(const struct bt_shell *sh, size_t argc, char *argv[])
{
	int err = 0;
	size_t len;
	uint32_t handle;

	len = strlen(argv[1]);

	if (len == (sizeof(handle) * 2)) {
		hex2bin(argv[1], len, (uint8_t *)&handle, sizeof(handle));
		sdp_discover.handle = sys_be32_to_cpu(handle);
	} else {
		bt_shell_session_error(sh, "Invalid UUID");
		return -ENOEXEC;
	}

	attr_ids.count = 0;

	if (argc > 2) {
		attr_ids.count = ARRAY_SIZE(attr_id_ranges);
		attr_ids.ranges = attr_id_ranges;
		attr_id_ranges[0].beginning = (uint16_t)bt_shell_strtol(argv[2], 0, &err);
		if (err < 0) {
			bt_shell_session_error(sh, "Invalid beginning ATTR ID");
			return -ENOEXEC;
		}
		attr_id_ranges[0].ending = 0xffff;
	}

	if (argc > 3) {
		attr_id_ranges[0].ending = (uint16_t)bt_shell_strtol(argv[3], 0, &err);
		if (err < 0) {
			bt_shell_session_error(sh, "Invalid ending ATTR ID");
			return -ENOEXEC;
		}
	}

	sdp_discover.func = sdp_discover_func;
	sdp_discover.pool = &sdp_client_pool;
	sdp_discover.type = BT_SDP_DISCOVER_SERVICE_ATTR;
	if (attr_ids.count != 0) {
		sdp_discover.ids = &attr_ids;
	}

	err = bt_sdp_discover(default_conn, &sdp_discover);
	if (err) {
		bt_shell_session_error(sh, "Fail to start SDP Discovery (err %d)", err);
		return err;
	}
	return 0;
}

/*
 * Attribute ID ranges for sa_discovery_ranges. Sized for one range more than a request PDU
 * carries: 32 ranges of 5 bytes plus the 2-byte sequence header make the 162-byte limit.
 */
#define ATTR_ID_RANGE_MAX_COUNT 32
static struct bt_sdp_attribute_id_range attr_id_range_list[ATTR_ID_RANGE_MAX_COUNT + 1];

static int cmd_sa_discovery_ranges(const struct bt_shell *sh, size_t argc, char *argv[])
{
	int err = 0;
	size_t len;
	uint32_t handle;
	unsigned long count;
	uint32_t step;

	len = strlen(argv[1]);

	if (len == (sizeof(handle) * 2)) {
		hex2bin(argv[1], len, (uint8_t *)&handle, sizeof(handle));
		sdp_discover.handle = sys_be32_to_cpu(handle);
	} else {
		bt_shell_session_error(sh, "Invalid handle");
		return -ENOEXEC;
	}

	count = bt_shell_strtoul(argv[2], 0, &err);
	if (err < 0 || count == 0 || count > ARRAY_SIZE(attr_id_range_list)) {
		bt_shell_session_error(sh, "Invalid range count");
		return -ENOEXEC;
	}

	/* Split the attribute ID space into ascending, non-overlapping ranges */
	step = 0x10000 / count;
	for (size_t i = 0; i < count; i++) {
		attr_id_range_list[i].beginning = (uint16_t)(i * step);
		attr_id_range_list[i].ending =
			(i == count - 1) ? 0xffff : (uint16_t)((i + 1) * step - 1);
	}

	attr_ids.count = count;
	attr_ids.ranges = attr_id_range_list;

	sdp_discover.func = sdp_discover_func;
	sdp_discover.pool = &sdp_client_pool;
	sdp_discover.type = BT_SDP_DISCOVER_SERVICE_ATTR;
	sdp_discover.ids = &attr_ids;

	err = bt_sdp_discover(default_conn, &sdp_discover);
	if (err != 0) {
		bt_shell_session_error(sh, "Fail to start SDP Discovery (err %d)", err);
		return err;
	}

	return 0;
}

static uint8_t sdp_discover_fail_func(struct bt_conn *conn, struct bt_sdp_client_result *result,
				      const struct bt_sdp_discover_params *params)
{
	if ((result == NULL) || (result->resp_buf == NULL) || (result->resp_buf->len == 0)) {
		bt_log_print("test pass\n");
	} else {
		bt_log_print("test fail\n");
	}

	return BT_SDP_DISCOVER_UUID_STOP;
}

static int cmd_ssa_discovery_fail(const struct bt_shell *sh, size_t argc, char *argv[])
{
	int err;

	sdp_discover.uuid = BT_UUID_DECLARE_16(BT_SDP_HANDSFREE_SVCLASS),
	sdp_discover.func = sdp_discover_fail_func;
	sdp_discover.pool = &sdp_client_pool;
	sdp_discover.type = BT_SDP_DISCOVER_SERVICE_SEARCH_ATTR;

	err = bt_sdp_discover(default_conn, &sdp_discover);
	if (err) {
		bt_shell_session_error(sh, "Fail to start SDP Discovery (err %d)", err);
		return err;
	}
	return 0;
}

#define HELP_ATTR_ID_LIST " [start] [end]"

BT_SHELL_STATIC_SUBCMD_SET_CREATE(sdp_client_cmds,
	BT_SHELL_CMD_ARG(ss_discovery, NULL, "<Big endian UUID>", cmd_ss_discovery, 2, 0),
	BT_SHELL_CMD_ARG(sa_discovery, NULL, "<Service Record Handle>" HELP_ATTR_ID_LIST,
		      cmd_sa_discovery, 2, 2),
	BT_SHELL_CMD_ARG(ssa_discovery, NULL, "<Big endian UUID>" HELP_ATTR_ID_LIST,
		      cmd_ssa_discovery, 2, 2),
	BT_SHELL_CMD_ARG(sa_discovery_ranges, NULL, "<Service Record Handle> <range count>",
		      cmd_sa_discovery_ranges, 3, 0),
	BT_SHELL_CMD_ARG(ssa_discovery_fail, NULL, "", cmd_ssa_discovery_fail, 1, 0),
	BT_SHELL_SUBCMD_SET_END
);

static int cmd_default_handler(const struct bt_shell *sh, size_t argc, char **argv)
{
	if (argc == 1) {
		bt_shell_help(sh);
		return BT_SHELL_CMD_HELP_PRINTED;
	}

	bt_shell_session_error(sh, "%s unknown parameter: %s", argv[0], argv[1]);

	return -EINVAL;
}

BT_SHELL_CMD_REGISTER(sdp_client, &sdp_client_cmds, "Bluetooth classic SDP client shell commands",
		   cmd_default_handler);

static void storage_init_d6528a887e(void)
{
	net_buf_pool_init(&sdp_client_pool);
}

int test_classic_sdp_c_commands_init(struct bt_shell *sh)
{
	net_buf_pool_init(&sdp_client_pool);
	if (bt_shell_register(sh, &bt_shell_root_sdp_client)) { return -EINVAL; }
	return 0;
}
