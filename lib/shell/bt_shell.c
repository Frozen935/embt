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
 * @brief Explicit command registration, recursive dispatch and bounded tokenization.
 */

#include <shell/bt_shell.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

void bt_shell_init(struct bt_shell *sh, FILE *output)
{
	*sh = (struct bt_shell){.output = output};
}

int bt_shell_register(struct bt_shell *sh, const struct bt_shell_cmd_entry *command)
{
	if (!sh || !command || !command->syntax || !command->syntax[0]) {
		return -EINVAL;
	}
	for (size_t i = 0; i < sh->root_count; i++) {
		if (!strcmp(sh->roots[i]->syntax, command->syntax)) {
			return -EEXIST;
		}
	}
	if (sh->root_count == BT_SHELL_MAX_ROOTS) {
		return -ENOSPC;
	}
	sh->roots[sh->root_count++] = command;
	return 0;
}

int bt_shell_tokenize(char *line, char **argv, size_t capacity)
{
	char *read = line;
	char *write = line;
	size_t count = 0;

	if (!line || !argv || capacity == 0) {
		return -EINVAL;
	}
	while (*read) {
		char quote = 0;

		while (isspace((unsigned char)*read)) {
			read++;
		}
		if (!*read) {
			break;
		}
		if (count + 1 >= capacity) {
			return -E2BIG;
		}
		argv[count++] = write;
		while (*read && (quote || !isspace((unsigned char)*read))) {
			char value = *read++;

			if (value == '\\' && quote != '\'') {
				if (!*read) {
					return -EINVAL;
				}
				*write++ = *read++;
			} else if (value == quote) {
				quote = 0;
			} else if (!quote && (value == '\'' || value == '"')) {
				quote = value;
			} else {
				*write++ = value;
			}
		}
		if (quote) {
			return -EINVAL;
		}
		/* Consume the delimiter before writing NUL: uncompacted read may equal write. */
		while (isspace((unsigned char)*read)) {
			read++;
		}
		*write++ = '\0';
	}
	argv[count] = NULL;
	return (int)count;
}

static const struct bt_shell_cmd_entry *root_find(const struct bt_shell *sh, const char *syntax)
{
	for (size_t i = 0; i < sh->root_count; i++) {
		if (!strcmp(sh->roots[i]->syntax, syntax)) {
			return sh->roots[i];
		}
	}
	return NULL;
}

static const struct bt_shell_cmd_entry *child_find(const struct bt_shell_cmd_entry *entry,
						   const char *syntax)
{
	if (entry->subcmd) {
		for (const struct bt_shell_cmd_entry *child = entry->subcmd->entries; child->syntax;
		     child++) {
			if (!strcmp(child->syntax, syntax)) {
				return child;
			}
		}
	}
	return NULL;
}

void bt_shell_vfprintf(const struct bt_shell *sh, enum bt_shell_color color, const char *fmt,
		       va_list args)
{
	FILE *stream = sh && sh->output ? sh->output : stdout;

	(void)color;
	flockfile(stream);
	vfprintf(stream, fmt, args);
	fflush(stream);
	funlockfile(stream);
}

void bt_shell_printf(const struct bt_shell *sh, enum bt_shell_color color, const char *fmt, ...)
{
	FILE *stream = sh && sh->output ? sh->output : stdout;
	va_list args;

	(void)color;
	va_start(args, fmt);
	flockfile(stream);
	vfprintf(stream, fmt, args);
	fputc('\n', stream);
	fflush(stream);
	funlockfile(stream);
	va_end(args);
}

void bt_shell_help(const struct bt_shell *sh)
{
	const struct bt_shell_cmd_entry *entry = sh->active;

	if (!entry) {
		for (size_t i = 0; i < sh->root_count; i++) {
			bt_shell_session_print(sh, "%-12s %s", sh->roots[i]->syntax,
					       sh->roots[i]->help ? sh->roots[i]->help : "");
		}
		return;
	}
	bt_shell_session_print(sh, "%s  %s", entry->syntax, entry->help ? entry->help : "");
	if (entry->subcmd) {
		for (const struct bt_shell_cmd_entry *child = entry->subcmd->entries; child->syntax;
		     child++) {
			bt_shell_session_print(sh, "  %-22s %s", child->syntax,
					       child->help ? child->help : "");
		}
	}
}

int bt_shell_help_path(struct bt_shell *sh, size_t argc, char **argv)
{
	const struct bt_shell_cmd_entry *entry = NULL;

	for (size_t i = 0; i < argc; i++) {
		entry = i == 0 ? root_find(sh, argv[i]) : child_find(entry, argv[i]);
		if (!entry) {
			return -ENOENT;
		}
	}
	sh->active = entry;
	bt_shell_help(sh);
	return BT_SHELL_CMD_HELP_PRINTED;
}

int bt_shell_exec(struct bt_shell *sh, size_t argc, char **argv)
{
	const struct bt_shell_cmd_entry *entry;
	size_t offset = 0;
	size_t remaining;

	if (argc == 0) {
		return 0;
	}
	if (argc > BT_SHELL_MAX_ARGS) {
		return -E2BIG;
	}
	entry = root_find(sh, argv[0]);
	if (!entry) {
		return -ENOENT;
	}
	while (entry->subcmd && offset + 1 < argc) {
		const struct bt_shell_cmd_entry *child = child_find(entry, argv[offset + 1]);

		if (!child) {
			if (!strcmp(argv[offset + 1], "--help") ||
			    !strcmp(argv[offset + 1], "-h")) {
				if (offset + 2 != argc) {
					return -EINVAL;
				}
				sh->active = entry;
				bt_shell_help(sh);
				return BT_SHELL_CMD_HELP_PRINTED;
			}
			return -ENOENT;
		}
		entry = child;
		offset++;
	}
	sh->active = entry;
	remaining = argc - offset;
	if (remaining == 2 &&
	    (!strcmp(argv[offset + 1], "--help") || !strcmp(argv[offset + 1], "-h"))) {
		bt_shell_help(sh);
		return BT_SHELL_CMD_HELP_PRINTED;
	}
	if (entry->mandatory &&
	    (remaining < entry->mandatory || remaining - entry->mandatory > entry->optional)) {
		bt_shell_help(sh);
		return -EINVAL;
	}
	if (!entry->handler) {
		bt_shell_help(sh);
		return BT_SHELL_CMD_HELP_PRINTED;
	}
	return entry->handler(sh, remaining, &argv[offset]);
}

void bt_shell_hexdump_to(const struct bt_shell *sh, const void *data, size_t size)
{
	const unsigned char *bytes = data;
	FILE *stream = sh && sh->output ? sh->output : stdout;

	flockfile(stream);
	for (size_t i = 0; i < size; i++) {
		fprintf(stream, "%02x%s", bytes[i], i % 16 == 15 || i + 1 == size ? "\n" : " ");
	}
	fflush(stream);
	funlockfile(stream);
}

long bt_shell_strtol(const char *text, int base, int *error)
{
	char *end;
	long result;

	if (*error) {
		return 0;
	}
	errno = 0;
	result = strtol(text, &end, base);
	if (errno || end == text || *end) {
		*error = errno == ERANGE ? -ERANGE : -EINVAL;
		return 0;
	}
	return result;
}

unsigned long bt_shell_strtoul(const char *text, int base, int *error)
{
	char *end;
	unsigned long result;

	if (*error) {
		return 0;
	}
	while (isspace((unsigned char)*text)) {
		text++;
	}
	if (*text == '-') {
		*error = -EINVAL;
		return 0;
	}
	errno = 0;
	result = strtoul(text, &end, base);
	if (errno || end == text || *end) {
		*error = errno == ERANGE ? -ERANGE : -EINVAL;
		return 0;
	}
	return result;
}

bool bt_shell_strtobool(const char *text, int base, int *error)
{
	if (*error) {
		return false;
	}
	if (!strcmp(text, "on") || !strcmp(text, "enable") || !strcmp(text, "true")) {
		return true;
	}
	if (!strcmp(text, "off") || !strcmp(text, "disable") || !strcmp(text, "false")) {
		return false;
	}
	return bt_shell_strtoul(text, base, error) != 0;
}

void bt_shell_fprintf_to(const struct bt_shell *sh, enum bt_shell_color color,
			 const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	bt_shell_vfprintf(sh, color, fmt, args);
	va_end(args);
}

unsigned long long bt_shell_strtoull(const char *text, int base, int *error)
{
	char *end;
	unsigned long long value;

	if (!error || *error) {
		return 0;
	}
	if (text) {
		while (isspace((unsigned char)*text)) {
			text++;
		}
	}
	if (!text || !*text || *text == '-') {
		*error = -EINVAL;
		return 0;
	}
	errno = 0;
	value = strtoull(text, &end, base);
	if (errno || *end || end == text) {
		*error = errno == ERANGE ? -ERANGE : -EINVAL;
		return 0;
	}
	return value;
}
