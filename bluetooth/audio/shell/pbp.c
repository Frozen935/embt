/** @file
 *  @brief Bluetooth Public Broadcast Profile shell
 */

/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <embt/base/assert.h>
#include <embt/base/util_macro.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <embt/bluetooth/assigned_numbers.h>
#include <embt/bluetooth/audio/audio.h>
#include <embt/bluetooth/audio/pbp.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/data.h>
#include <embt/bluetooth/gap.h>
#include <embt/net_buf.h>
#include <shell/bt_shell.h>
#include <embt/base/assert.h>
#include <embt/base/util.h>
#include <embt/compiler.h>

#include "common/bt_shell_private.h"

#define PBS_DEMO                'P', 'B', 'P'

static const uint8_t pba_metadata[] = {
	BT_AUDIO_CODEC_DATA(BT_AUDIO_METADATA_TYPE_PROGRAM_INFO, PBS_DEMO)};

/* Buffer to hold the Public Broadcast Announcement */
NET_BUF_SIMPLE_DEFINE_STATIC(pbp_ad_buf, BT_PBP_MIN_PBA_SIZE + ARRAY_SIZE(pba_metadata));

static enum bt_pbp_announcement_feature pbp_features;

static int cmd_pbp_set_features(const struct bt_shell *sh, size_t argc, char **argv)
{
	int err = 0;
	enum bt_pbp_announcement_feature features;

	ARG_UNUSED(argc);

	features = bt_shell_strtoul(argv[1], 16, &err);
	if (err != 0) {
		bt_shell_session_error(sh, "Could not parse received features: %d", err);

		return -ENOEXEC;
	}

	pbp_features = features;

	return err;
}

size_t pbp_ad_data_add(struct bt_data data[], size_t data_size)
{
	int err;

	net_buf_simple_reset(&pbp_ad_buf);

	err = bt_pbp_get_announcement(pba_metadata,
				      ARRAY_SIZE(pba_metadata),
				      pbp_features,
				      &pbp_ad_buf);

	if (err != 0) {
		bt_shell_info("Create Public Broadcast Announcement");
	} else {
		bt_shell_error("Failed to create Public Broadcast Announcement: %d", err);
	}

	__ASSERT(data_size > 0, "No space for Public Broadcast Announcement");
	data[0].type = BT_DATA_SVC_DATA16;
	data[0].data_len = pbp_ad_buf.len;
	data[0].data = pbp_ad_buf.data;

	return 1U;
}

static int cmd_pbp(const struct bt_shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		bt_shell_session_error(sh, "%s unknown parameter: %s", argv[0], argv[1]);
	} else {
		bt_shell_session_error(sh, "%s missing subcomand", argv[0]);
	}

	return -ENOEXEC;
}

BT_SHELL_STATIC_SUBCMD_SET_CREATE(pbp_cmds,
	BT_SHELL_CMD_ARG(set_features, NULL,
		      "Set the Public Broadcast Announcement features",
		      cmd_pbp_set_features, 2, 0),
	BT_SHELL_SUBCMD_SET_END
);

BT_SHELL_CMD_ARG_REGISTER(pbp, &pbp_cmds, "Bluetooth pbp shell commands", cmd_pbp, 1, 1);
