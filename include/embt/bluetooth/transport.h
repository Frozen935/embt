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
 * @brief Explicit selection of HCI sockets and platform H4 device transports.
 */

#ifndef EMBT_BLUETOOTH_TRANSPORT_H_
#define EMBT_BLUETOOTH_TRANSPORT_H_
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @brief HCI connection endpoint type. */
enum bt_hci_socket_kind {
	BT_HCI_SOCKET_USER, /**< Linux HCI_CHANNEL_USER with exclusive controller ownership. */
	BT_HCI_SOCKET_TCP,  /**< H4 byte stream over a TCP client connection. */
	BT_HCI_SOCKET_UNIX, /**< H4 byte stream over a Unix stream client connection. */
};
/** @brief Endpoint copied during configuration; no socket is opened yet. */
struct bt_hci_socket_config {
	enum bt_hci_socket_kind kind;
	const char *address; /**< TCP host or absolute Unix path; unused for USER. */
	uint16_t port;       /**< TCP port; unused for other kinds. */
	uint16_t controller; /**< Linux HCI controller index for USER. */
	uint32_t quirks;     /**< BT_HCI_QUIRK_* flags, zero for normal controllers. */
};
/**
 * @brief Select the socket transport before bt_enable().
 * @param[in] config Endpoint to copy; must not be NULL.
 * @return Zero on success or a negative errno value.
 * @pre Call from the serialized stack lifecycle owner while the stack is disabled.
 */
int bt_hci_socket_configure(const struct bt_hci_socket_config *config);
/**
 * @brief Select a platform H4 device before bt_enable().
 * @param[in] path Absolute device path copied by the driver.
 * @param[in] quirks Explicit BT_HCI_QUIRK_* flags.
 * @return Zero on success or a negative errno value.
 */
int bt_hci_h4_configure(const char *path, uint32_t quirks);
#ifdef __cplusplus
}
#endif
#endif
