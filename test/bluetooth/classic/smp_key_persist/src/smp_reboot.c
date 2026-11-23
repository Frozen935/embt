#include "support/test_support.h"
/* smp_br_io_cap.c - Bluetooth classic SMP IO CAP smoke test */

/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <embt/base/byteorder.h>




#include <shell/bt_shell.h>

#include <host/shell/bt.h>
#include <common/bt_shell_private.h>

static int cmd_reboot(const struct bt_shell *sh, size_t argc, char *argv[])
{
	fflush(NULL);
	_Exit(75);

	return 0;
}

#define HELP_NONE "[none]"

BT_SHELL_STATIC_SUBCMD_SET_CREATE(
	test_smp_cmds,
	BT_SHELL_CMD_ARG(reboot, NULL, HELP_NONE, cmd_reboot, 1, 0),
	BT_SHELL_SUBCMD_SET_END);

static int cmd_default_handler(const struct bt_shell *sh, size_t argc, char **argv)
{
	bt_shell_session_error(sh, "%s unknown parameter: %s", argv[0], argv[1]);

	return -EINVAL;
}

BT_SHELL_CMD_REGISTER(test_smp, &test_smp_cmds, "smp test cmds", cmd_default_handler);

int test_classic_smp_key_persist_commands_init(struct bt_shell *sh)
{
	if (bt_shell_register(sh, &bt_shell_root_test_smp)) { return -EINVAL; }
	return 0;
}
