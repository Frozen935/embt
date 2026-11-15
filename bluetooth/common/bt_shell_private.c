/**
 * @file bt_shell_private.c
 * @brief Bluetooth shell private module
 *
 * Provide common function which can be shared using privately inside Bluetooth shell.
 */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_shell_private.h"

static void wall_vfprintf(enum bt_shell_color color, const char *fmt, va_list args)
{
	bt_shell_vfprintf(NULL, color, fmt, args);
}

void bt_shell_hexdump(const uint8_t *data, size_t len)
{
	bt_shell_hexdump_to(NULL, data, len);
}

void bt_shell_fprintf(enum bt_shell_color color,
		      const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(color, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_INFO, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_NORMAL, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_WARNING, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_ERROR, fmt, args);
	va_end(args);
}

extern const struct bt_shell_cmd_entry bt_shell_root_bt;
extern const struct bt_shell_cmd_entry bt_shell_root_gatt;
extern const struct bt_shell_cmd_entry bt_shell_root_l2cap;
extern const struct bt_shell_cmd_entry bt_shell_root_br;
int bt_shell_bt_callbacks_init(void);
int bt_shell_l2cap_callbacks_init(void);

int bt_shell_commands_init(struct bt_shell *sh)
{
	int err;

	err = bt_shell_register(sh, &bt_shell_root_bt);
	if (err) {
		return err;
	}
#if defined(CONFIG_BT_CONN)
	err = bt_shell_register(sh, &bt_shell_root_gatt);
	if (err) {
		return err;
	}
#endif
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	err = bt_shell_register(sh, &bt_shell_root_l2cap);
	if (err) {
		return err;
	}
#endif
#if defined(CONFIG_BT_CLASSIC)
	err = bt_shell_register(sh, &bt_shell_root_br);
	if (err) {
		return err;
	}
#endif
	err = bt_shell_bt_callbacks_init();
	if (err) {
		return err;
	}
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	return bt_shell_l2cap_callbacks_init();
#else
	return 0;
#endif
}
