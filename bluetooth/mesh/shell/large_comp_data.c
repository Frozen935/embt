/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <shell/bt_shell.h>
#include <embt/bluetooth/mesh.h>
#include <embt/bluetooth/mesh/shell.h>

#include "common/bt_shell_private.h"
#include "utils.h"

static void status_print(int err, char *msg, uint16_t addr, struct bt_mesh_large_comp_data_rsp *rsp)
{
	if (err) {
		bt_shell_error("Failed to send %s Get message (err %d)", msg, err);
		return;
	}

	bt_shell_print("%s [0x%04x]: page: %u offset: %u total size: %u", msg, addr, rsp->page,
		       rsp->offset, rsp->total_size);
	bt_shell_hexdump(rsp->data->data, rsp->data->len);
}

static int cmd_large_comp_data_get(const struct bt_shell *sh, size_t argc, char *argv[])
{
	NET_BUF_SIMPLE_DEFINE(comp, 64);
	struct bt_mesh_large_comp_data_rsp rsp = {
		.data = &comp,
	};
	uint8_t page;
	uint16_t offset;
	int err = 0;

	net_buf_simple_init(&comp, 0);

	page = bt_shell_strtoul(argv[1], 0, &err);
	offset = bt_shell_strtoul(argv[2], 0, &err);

	if (err) {
		bt_shell_session_warn(sh, "Unable to parse input string argument");
		return err;
	}

	err = bt_mesh_large_comp_data_get(bt_mesh_shell_target_ctx.net_idx,
					  bt_mesh_shell_target_ctx.dst, page, offset, &rsp);
	status_print(err, "Composition Data", bt_mesh_shell_target_ctx.dst, &rsp);

	return err;
}

static int cmd_models_metadata_get(const struct bt_shell *sh, size_t argc, char *argv[])
{
	NET_BUF_SIMPLE_DEFINE(metadata, 64);
	struct bt_mesh_large_comp_data_rsp rsp = {
		.data = &metadata,
	};
	uint8_t page;
	uint16_t offset;
	int err = 0;

	net_buf_simple_init(&metadata, 0);

	page = bt_shell_strtoul(argv[1], 0, &err);
	offset = bt_shell_strtoul(argv[2], 0, &err);

	if (err) {
		bt_shell_session_warn(sh, "Unable to parse input string argument");
		return err;
	}

	err = bt_mesh_models_metadata_get(bt_mesh_shell_target_ctx.net_idx,
					  bt_mesh_shell_target_ctx.dst, page, offset, &rsp);
	status_print(err, "Models Metadata", bt_mesh_shell_target_ctx.dst, &rsp);

	return err;
}

const struct bt_shell_cmd_entry large_comp_data_cmds_entries[] = {
	BT_SHELL_CMD_ARG(large-comp-data-get, NULL, "<page> <offset>", cmd_large_comp_data_get, 3, 0),
	BT_SHELL_CMD_ARG(models-metadata-get, NULL, "<page> <offset>", cmd_models_metadata_get, 3, 0),
	BT_SHELL_SUBCMD_SET_END};
