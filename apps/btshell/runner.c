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
 * @brief POSIX btshell application entry with explicit transport selection and bounded input.
 */

#include "runner.h"
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/transport.h>
#include <embt/drivers/bluetooth.h>
#if defined(CONFIG_EMBT_AUDIO_SHELL)
#include <embt/drivers/audio.h>
#if defined(CONFIG_EMBT_BAP_PLAYER)
#include <audio/shell/audio.h>
#endif
#endif
#if defined(CONFIG_SETTINGS)
#include <embt/settings/settings.h>
#endif
#include "common/bt_shell_private.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void usage(FILE *output)
{
	fputs("Usage: btshell", output);
#if defined(CONFIG_EMBT_HCI_H4)
	fputs(" [--h4 PATH]", output);
#endif
#if defined(CONFIG_EMBT_HCI_SOCKET)
	fputs(" [--hci INDEX | --tcp HOST PORT | --unix PATH]", output);
#endif
	fputs(" [--no-flow-control]\n"
	      "       btshell --help | --version\n"
	      "Select a transport without opening it. Bluetooth starts only on an "
	      "explicit command.\n"
	      "--no-flow-control selects the Controller quirk for rejected Host flow-control "
	      "commands.\n", output);
#if defined(CONFIG_EMBT_HCI_H4)
	fprintf(output, "H4 default device: %s\n", CONFIG_EMBT_HCI_H4_DEVICE);
#endif
}

static int configure(int argc, char **argv)
{
#if defined(CONFIG_EMBT_HCI_SOCKET)
	struct bt_hci_socket_config config = {0};
	unsigned long number;
	int err = 0;
#endif
	bool no_flow_control = false;

	if (argc > 1 && !strcmp(argv[argc - 1], "--no-flow-control")) {
		no_flow_control = true;
		argc--;
	}
#if defined(CONFIG_EMBT_HCI_H4)
	if (argc == 1 || (argc == 3 && !strcmp(argv[1], "--h4"))) {
		const char *path = argc == 1 ? CONFIG_EMBT_HCI_H4_DEVICE : argv[2];

		return bt_hci_h4_configure(path,
			no_flow_control ? BT_HCI_QUIRK_NO_FLOW_CONTROL : 0);
	}
#endif
	if (argc == 1) {
		return no_flow_control ? -EINVAL : 0;
	}
#if defined(CONFIG_EMBT_HCI_SOCKET)
	if (argc == 3 && !strcmp(argv[1], "--hci")) {
		number = bt_shell_strtoul(argv[2], 10, &err);
		if (err || number >= UINT16_MAX) {
			return -EINVAL;
		}
		config.kind = BT_HCI_SOCKET_USER;
		config.controller = (uint16_t)number;
	} else if (argc == 4 && !strcmp(argv[1], "--tcp")) {
		number = bt_shell_strtoul(argv[3], 10, &err);
		if (err || number == 0 || number > UINT16_MAX) {
			return -EINVAL;
		}
		config.kind = BT_HCI_SOCKET_TCP;
		config.address = argv[2];
		config.port = (uint16_t)number;
	} else if (argc == 3 && !strcmp(argv[1], "--unix")) {
		config.kind = BT_HCI_SOCKET_UNIX;
		config.address = argv[2];
	} else {
		return -EINVAL;
	}
	if (no_flow_control) {
		config.quirks |= BT_HCI_QUIRK_NO_FLOW_CONTROL;
	}
	return bt_hci_socket_configure(&config);
#else
	return -EINVAL;
#endif
}

/** Read one bounded line; discard overflow so the suffix cannot become a command. */
static int read_line(char *line, size_t size)
{
	size_t length = 0;
	bool overflow = false;
	bool binary = false;
	int ch;

	while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
		if (ch == '\0') {
			binary = true;
		}
		if (length + 1 < size) {
			line[length++] = (char)ch;
		} else {
			overflow = true;
		}
	}
	if (ferror(stdin)) {
		return -EIO;
	}
	if (ch == EOF && length == 0 && !overflow) {
		return 0;
	}
	line[length] = '\0';
	return binary ? -EINVAL : (overflow ? -E2BIG : 1);
}

static void print_help(struct bt_shell *sh)
{
	bt_shell_session_print(sh,
			       "help [command path]  Show help without invoking command handlers");
	bt_shell_session_print(sh, "version              Show the tool version");
	bt_shell_session_print(sh, "quit | exit | q      Exit btshell");
	bt_shell_help_path(sh, 0, NULL);
}

int bt_shell_app_run(int argc, char **argv,
		    int (*register_commands)(struct bt_shell *sh))
{
	struct bt_shell sh;
	char line[BT_SHELL_MAX_LINE + 1];
	char *arguments[BT_SHELL_MAX_ARGS + 1];
	bool interactive = isatty(STDIN_FILENO);
	int err;

	if (argc == 2 && !strcmp(argv[1], "--help")) {
		usage(stdout);
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "--version")) {
		puts("EmBT btshell");
		return 0;
	}
	err = configure(argc, argv);
	if (err) {
		usage(stderr);
		fprintf(stderr, "Invalid transport configuration (%d)\n", err);
		return 2;
	}
	err = bt_stack_init();
	if (err) {
		fprintf(stderr, "Stack mechanism initialization failed (%d)\n", err);
		return 1;
	}
#if defined(CONFIG_SETTINGS)
	err = settings_subsys_init();
	if (err) {
		fprintf(stderr, "Settings initialization failed (%d)\n", err);
		return 1;
	}
#endif
	bt_shell_init(&sh, stdout);
#if defined(CONFIG_EMBT_AUDIO_SHELL)
	err = bt_audio_driver_init();
	if (!err) {
		err = bt_audio_pulse_register();
	}
	if (err) {
		bt_audio_driver_shutdown();
		fprintf(stderr, "Audio initialization failed (%d)\n", err);
		return 1;
	}
#endif
	if (bt_shell_storage_init()) {
		return 1;
	}
	err = bt_shell_commands_init(&sh);
	if (!err && register_commands) {
		err = register_commands(&sh);
	}
	if (err) {
		fprintf(stderr, "Command registration failed (%d)\n", err);
		return 1;
	}
	puts("EmBT btshell. Type 'help' for commands. Bluetooth is disabled.");
	for (;;) {
		int count;

		if (interactive) {
			fputs("btshell> ", stdout);
			fflush(stdout);
		}
		err = read_line(line, sizeof(line));
		if (err == 0) {
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Invalid command line (%d)\n", err);
			if (err == -EIO) {
				return 1;
			}
			continue;
		}
		count = bt_shell_tokenize(line, arguments, ARRAY_SIZE(arguments));
		if (count < 0) {
			fprintf(stderr, "Invalid command arguments (%d)\n", count);
			continue;
		}
		if (count == 0) {
			continue;
		}
		if (!strcmp(arguments[0], "quit") || !strcmp(arguments[0], "exit") ||
		    !strcmp(arguments[0], "q")) {
			if (count != 1) {
				fputs("Exit takes no arguments\n", stderr);
				continue;
			}
			break;
		}
		if (!strcmp(arguments[0], "help")) {
			if (count == 1) {
				print_help(&sh);
				continue;
			}
			err = bt_shell_help_path(&sh, count - 1, &arguments[1]);
		} else if (!strcmp(arguments[0], "version") && count == 1) {
			puts("EmBT btshell");
			continue;
		} else {
			err = bt_shell_exec(&sh, (size_t)count, arguments);
		}
		if (err < 0) {
			fprintf(stderr, "Command failed: %s (%d)\n", strerror(-err), err);
		}
	}
	/* Process-lifetime stack workers end with the tool. No HCI operation is
	 * issued implicitly on exit; wireless teardown remains a runtime task.
	 */
	puts("Bye.");
#if defined(CONFIG_EMBT_AUDIO_SHELL)
#if defined(CONFIG_EMBT_BAP_PLAYER)
	err = bt_audio_shell_bap_playback_shutdown();
	if (err) {
		fprintf(stderr, "BAP playback cleanup failed (%d)\n", err);
	}
#endif
	bt_audio_driver_shutdown();
#endif
	return 0;
}
