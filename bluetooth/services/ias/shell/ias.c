/**
 * @file
 * @brief Shell APIs for Bluetooth IAS
 *
 * Copyright (c) 2022 Codecoup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <embt/bluetooth/conn.h>

#include <shell/bt_shell.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/services/ias.h>

#include "host/shell/bt.h"
#include "common/bt_shell_private.h"

static void alert_stop(void)
{
	bt_shell_print("Alert stopped\n");
}

static void alert_start(void)
{
	bt_shell_print("Mild alert started\n");
}

static void alert_high_start(void)
{
	bt_shell_print("High alert started\n");
}

BT_IAS_CB_DEFINE(ias_callbacks) = {
	.no_alert = alert_stop,
	.mild_alert = alert_start,
	.high_alert = alert_high_start,
};

static int cmd_ias_local_alert_stop(const struct bt_shell *sh, size_t argc, char **argv)
{
	const int result = bt_ias_local_alert_stop();

	if (result) {
		bt_shell_session_print(sh, "Local alert stop failed: %d", result);
	} else {
		bt_shell_session_print(sh, "Local alert stopped");
	}

	return result;
}

static int cmd_ias(const struct bt_shell *sh, size_t argc, char **argv)
{
	if (argc > 1) {
		bt_shell_session_error(sh, "%s unknown parameter: %s", argv[0], argv[1]);
	} else {
		bt_shell_session_error(sh, "%s Missing subcommand", argv[0]);
	}

	return -ENOEXEC;
}

BT_SHELL_STATIC_SUBCMD_SET_CREATE(ias_cmds,
	BT_SHELL_CMD_ARG(local_alert_stop, NULL,
		      "Stop alert locally",
		      cmd_ias_local_alert_stop, 1, 0),
	BT_SHELL_SUBCMD_SET_END
);

BT_SHELL_CMD_ARG_REGISTER(ias, &ias_cmds, "Bluetooth IAS shell commands",
		       cmd_ias, 1, 1);
