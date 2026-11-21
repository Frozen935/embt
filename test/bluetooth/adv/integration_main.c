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

/** @file @brief Physical Controller advertising regression entrypoint. */
#include "support/test_support.h"
#include <embt/bluetooth/transport.h>
#include <embt/drivers/bluetooth.h>
extern const struct bt_test_suite bt_test_suite_adv_tests;
int main(void)
{
 struct bt_hci_socket_config config = { .kind = BT_HCI_SOCKET_USER, .controller = 0,
  .quirks = BT_HCI_QUIRK_NO_FLOW_CONTROL };
 if (bt_hci_socket_configure(&config) || bt_test_initialize()) { return 1; }
 return bt_test_run_suite(&bt_test_suite_adv_tests, NULL);
}
