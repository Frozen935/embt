/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/**
 * @file
 * @brief Bounded standalone command trees, parsing and output for internal Bluetooth tools.
 */

#ifndef EMBT_INTERNAL_BT_SHELL_H_
#define EMBT_INTERNAL_BT_SHELL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <embt/compiler.h>
#include <embt/base/util.h>

/** @brief Input and command bounds for the standalone shell. */
#define BT_SHELL_MAX_ARGS         64
#define BT_SHELL_MAX_LINE         4096
#define BT_SHELL_MAX_ROOTS        64
#define BT_SHELL_CMD_HELP_PRINTED 1

struct bt_shell;
struct bt_shell_cmd_entry;
/** @brief Immutable, explicitly referenced subcommand table. */
struct bt_shell_cmd_set {
	const struct bt_shell_cmd_entry *entries;
};
/**
 * @brief Execute one command after argument validation.
 * @param[in] sh Serialized session whose active descriptor matches this handler.
 * @param[in] argc Argument count, including the selected command name.
 * @param[in] argv Mutable argument pointers, valid only until the handler returns.
 * @return Handler-defined status, normally zero or a negative errno value.
 */
typedef int (*bt_shell_handler_t)(const struct bt_shell *sh, size_t argc, char **argv);
/** @brief One command descriptor; no OS object or implicit registration is involved. */
struct bt_shell_cmd_entry {
	const char *syntax;
	const struct bt_shell_cmd_set *subcmd;
	const char *help;
	bt_shell_handler_t handler;
	size_t mandatory; /**< Zero leaves argument-count validation to the handler. */
	size_t optional;
};
/** @brief One serialized command session. Output remains valid through session teardown. */
struct bt_shell {
	FILE *output;
	const struct bt_shell_cmd_entry *roots[BT_SHELL_MAX_ROOTS];
	size_t root_count;
	const struct bt_shell_cmd_entry *active;
};
/** @brief Output intent; text is currently emitted without terminal escape sequences. */
enum bt_shell_color {
	BT_SHELL_NORMAL,
	BT_SHELL_INFO,
	BT_SHELL_WARNING,
	BT_SHELL_ERROR,
};

#define BT_SHELL_CMD_ARG(syntax_, sub_, help_, handler_, mandatory_, optional_)                    \
	{                                                                                          \
		.syntax = STRINGIFY(syntax_), .subcmd = (sub_), .help = (help_),                   \
				    .handler = (handler_), .mandatory = (mandatory_),              \
				    .optional = (optional_)                                        \
	}
#define BT_SHELL_CMD(syntax_, sub_, help_, handler_)                                               \
	BT_SHELL_CMD_ARG(syntax_, sub_, help_, handler_, 1, BT_SHELL_MAX_ARGS - 1)
#define BT_SHELL_SUBCMD_SET_END                                                                    \
	{                                                                                          \
		.syntax = NULL                                                                     \
	}
#define BT_SHELL_STATIC_SUBCMD_SET_CREATE(name, ...)                                               \
	static const struct bt_shell_cmd_entry name##_entries[] = {__VA_ARGS__};                   \
	static const struct bt_shell_cmd_set name = {.entries = name##_entries}
#define BT_SHELL_CMD_ARG_REGISTER(syntax_, sub_, help_, handler_, mandatory_, optional_)           \
	const struct bt_shell_cmd_entry bt_shell_root_##syntax_ =                                  \
		BT_SHELL_CMD_ARG(syntax_, sub_, help_, handler_, mandatory_, optional_)
#define BT_SHELL_CMD_REGISTER(syntax_, sub_, help_, handler_)                                      \
	BT_SHELL_CMD_ARG_REGISTER(syntax_, sub_, help_, handler_, 1, BT_SHELL_MAX_ARGS - 1)

/**
 * @brief Initialize empty session storage without invoking command handlers.
 * @param[out] sh Caller-owned session.
 * @param[in] output Borrowed output stream, live for the whole session.
 */
void bt_shell_init(struct bt_shell *sh, FILE *output);
/**
 * @brief Register a process-lifetime root descriptor in one session.
 * @param[in,out] sh Initialized session, used by one command thread.
 * @param[in] command Immutable root descriptor.
 * @return Zero, -EEXIST for duplicate syntax or -ENOSPC at the root limit.
 */
int bt_shell_register(struct bt_shell *sh, const struct bt_shell_cmd_entry *command);
/**
 * @brief Tokenize a mutable line with quotes and backslash escapes.
 * @param[in,out] line NUL-terminated input, compacted in place.
 * @param[out] argv Receives token pointers and a terminating NULL pointer.
 * @param[in] capacity Pointer slots, including the terminating NULL slot.
 * @return Token count, -E2BIG on overflow or -EINVAL for unmatched quote/escape.
 * @note Never call handlers after parsing fails; partially written output is invalid.
 */
int bt_shell_tokenize(char *line, char **argv, size_t capacity);
/**
 * @brief Dispatch recursively after validating the selected command's argument bounds.
 * @param[in,out] sh Serialized session.
 * @param[in] argc Token count.
 * @param[in] argv Token vector; handlers receive the matching command as argv[0].
 * @return Handler result, help marker, or a negative errno for malformed input/unknown command.
 */
int bt_shell_exec(struct bt_shell *sh, size_t argc, char **argv);
/**
 * @brief Print help for a command path without executing any protocol handler.
 * @param[in,out] sh Session whose active help target is updated.
 * @param[in] argc Path component count; zero lists roots.
 * @param[in] argv Path components such as br, l2cap.
 * @return Help marker or -ENOENT if the path does not exist.
 */
int bt_shell_help_path(struct bt_shell *sh, size_t argc, char **argv);
/**
 * @brief Print help for the active command without invoking any handler.
 * @param[in] sh Live session; a NULL active descriptor lists all roots.
 */
void bt_shell_help(const struct bt_shell *sh);
/**
 * @brief Emit formatted text under the output stream lock.
 * @param[in] sh Session, or NULL to use stdout for asynchronous notices.
 * @param[in] color Output intent; currently emitted without ANSI colors.
 * @param[in] fmt Format string, without an implicit newline.
 * @param[in] args Format arguments, consumed by this call.
 * @note Thread context. Caller-owned output streams must outlive all writers.
 */
__printf_like(3, 0) void bt_shell_vfprintf(const struct bt_shell *sh, enum bt_shell_color color,
					   const char *fmt, va_list args);
/**
 * @brief Emit one formatted line under the output stream lock.
 * @param[in] sh Session, or NULL to use stdout.
 * @param[in] color Output intent; currently emitted without ANSI colors.
 * @param[in] fmt Format string, with a newline appended by this function.
 * @param[in] ... Format arguments.
 */
__printf_like(3, 4) void bt_shell_printf(const struct bt_shell *sh, enum bt_shell_color color,
					 const char *fmt, ...);
/**
 * @brief Print a hexadecimal dump from thread context.
 * @param[in] sh Session, or NULL to use stdout.
 * @param[in] data Bytes to print; must be valid for size bytes.
 * @param[in] size Byte count; zero produces no output.
 */
void bt_shell_hexdump_to(const struct bt_shell *sh, const void *data, size_t size);
#define bt_shell_session_print(sh, ...) bt_shell_printf(sh, BT_SHELL_NORMAL, __VA_ARGS__)
#define bt_shell_session_info(sh, ...)  bt_shell_printf(sh, BT_SHELL_INFO, __VA_ARGS__)
#define bt_shell_session_warn(sh, ...)  bt_shell_printf(sh, BT_SHELL_WARNING, __VA_ARGS__)
#define bt_shell_session_error(sh, ...) bt_shell_printf(sh, BT_SHELL_ERROR, __VA_ARGS__)
/**
 * @brief Parse a complete signed integer while preserving an earlier error.
 * @param[in] text NUL-terminated number.
 * @param[in] base Numeric base accepted by strtol().
 * @param[in,out] error Required error accumulator; zero enables parsing.
 * @return Parsed value, or zero with -EINVAL/-ERANGE stored on failure.
 */
long bt_shell_strtol(const char *text, int base, int *error);
/**
 * @brief Parse a complete unsigned integer while rejecting negative input.
 * @param[in] text NUL-terminated number.
 * @param[in] base Numeric base accepted by strtoul().
 * @param[in,out] error Required error accumulator; an earlier nonzero error is preserved.
 * @return Parsed value, or zero with -EINVAL/-ERANGE stored on failure.
 */
unsigned long bt_shell_strtoul(const char *text, int base, int *error);
/**
 * @brief Parse on/off, enable/disable, true/false or an unsigned integer boolean.
 * @param[in] text NUL-terminated boolean spelling or number.
 * @param[in] base Numeric base for integer input.
 * @param[in,out] error Required error accumulator; an earlier nonzero error is preserved.
 * @return Parsed boolean; false on failure with error set.
 */
bool bt_shell_strtobool(const char *text, int base, int *error);

/** @brief Print formatted text without appending a newline. */
__printf_like(3, 4) void bt_shell_fprintf_to(const struct bt_shell *sh,
					enum bt_shell_color color, const char *fmt, ...);
/** @brief Parse a complete unsigned 64-bit value using the error accumulator. */
unsigned long long bt_shell_strtoull(const char *text, int base, int *error);

#define BT_SHELL_COND_CMD_ARG(flag, ...) \
	COND_CODE_1(flag, (BT_SHELL_CMD_ARG(__VA_ARGS__)), ())

#endif /* EMBT_INTERNAL_BT_SHELL_H_ */
