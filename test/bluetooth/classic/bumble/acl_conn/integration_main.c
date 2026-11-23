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

/** @file @brief Two-role Classic ACL integration entrypoint. */
#include "support/test_support.h"
#include <embt/bluetooth/transport.h>
#include <embt/bluetooth/addr.h>
extern bt_addr_t peer_addr;
extern const struct bt_test_suite bt_test_suite_acl_peripheral;
extern const struct bt_test_suite bt_test_suite_acl_central;
int main(int argc, char **argv)
{
	if (argc != 4) { return 2; }
	struct bt_hci_socket_config config = {
		.kind = BT_HCI_SOCKET_TCP, .address = "127.0.0.1",
		.port = (uint16_t)strtoul(argv[1], NULL, 10),
	};
	if (bt_addr_from_str(argv[2], &peer_addr) || bt_hci_socket_configure(&config) ||
	    bt_test_initialize()) { return 1; }
	if (!strcmp(argv[3], "peripheral")) { return bt_test_run_suite(&bt_test_suite_acl_peripheral, NULL); }
	if (!strcmp(argv[3], "central")) { return bt_test_run_suite(&bt_test_suite_acl_central, NULL); }
	return 2;
}
