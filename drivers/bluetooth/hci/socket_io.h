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
 * @brief POSIX socket I/O isolated from Host and BlueZ type namespaces.
 */

#ifndef EMBT_SOCKET_IO_H_
#define EMBT_SOCKET_IO_H_
#include <stddef.h>
#include <stdint.h>
#include <embt/bluetooth/transport.h>
/**
 * @brief Open a nonblocking endpoint.
 *
 * The Linux user endpoint brings the selected controller down with HCIDEVDOWN
 * before binding. It requires CAP_NET_ADMIN and CAP_NET_RAW. This does not stop
 * BlueZ or restore the adapter's powered state when the endpoint is closed.
 *
 * @param[in] config Endpoint to open; must remain valid for this call.
 * @return Socket descriptor or a negative errno value. After a failed bind,
 *   the controller may already be down; the lifecycle owner handles restoration.
 */
int bt_socket_connect(const struct bt_hci_socket_config *config);
/** @brief Receive one chunk or USER packet, observing a shutdown descriptor. */
int bt_socket_read(int fd, int stop_fd, void *data, size_t size, int packet);
/** @brief Send a complete H4 packet; stream partial writes are completed. */
int bt_socket_write(int fd, int stop_fd, const void *data, size_t size, int packet);
#endif
