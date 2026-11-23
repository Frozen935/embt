/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <shell/bt_shell.h>
#include <embt/bluetooth/mesh.h>
#include <embt/bluetooth/mesh/shell.h>

#include "utils.h"

static int cmd_od_priv_gatt_proxy_set(const struct bt_shell *sh, size_t argc,
				      char *argv[])
{
	uint8_t val, val_rsp;
	uint16_t net_idx = bt_mesh_shell_target_ctx.net_idx;
	uint16_t addr = bt_mesh_shell_target_ctx.dst;
	int err = 0;

	if (argc < 2) {
		err = bt_mesh_od_priv_proxy_cli_get(net_idx, addr, &val_rsp);
	} else {
		val = bt_shell_strtoul(argv[1], 0, &err);

		if (err) {
			bt_shell_session_warn(sh, "Unable to parse input string argument");
			return err;
		}

		err = bt_mesh_od_priv_proxy_cli_set(net_idx, addr, val, &val_rsp);
	}

	if (err) {
		bt_shell_session_print(sh, "Unable to send On-Demand Private GATT Proxy Get/Set (err %d)",
			    err);
		return 0;
	}

	bt_shell_session_print(sh, "On-Demand Private GATT Proxy is set to 0x%02x", val_rsp);

	return 0;
}

const struct bt_shell_cmd_entry od_priv_proxy_cmds_entries[] = {
	BT_SHELL_CMD_ARG(gatt-proxy, NULL, "[Dur(s)]", cmd_od_priv_gatt_proxy_set, 1, 1),
	BT_SHELL_SUBCMD_SET_END};
