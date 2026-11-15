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

/** @file @brief Internal btshell application entry shared by CLI tests. */
#ifndef EMBT_BTSHELL_RUNNER_H_
#define EMBT_BTSHELL_RUNNER_H_
#include <shell/bt_shell.h>

/**
 * @brief Configure the application, register commands and run its input loop.
 * @param argc Number of application arguments.
 * @param argv Application arguments, including transport selection.
 * @param register_commands Optional module-local resource/command initializer.
 * @return The application process result.
 * @note Bluetooth starts only after an explicit shell command.
 */
int bt_shell_app_run(int argc, char **argv,
		    int (*register_commands)(struct bt_shell *sh));
#endif
