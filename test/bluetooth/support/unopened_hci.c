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

/** @file @brief Unopened real transport descriptor for Controller-free buffer fixtures. */
#include "test_support.h"
#include <embt/drivers/bluetooth.h>
#if defined(CONFIG_BT_HCI_RAW)
#include <embt/bluetooth/hci_raw.h>
#include "host/hci_raw_internal.h"
#else
#include "host/hci_core.h"
#endif
static struct bt_hci_driver_data driver_data;
static int unexpected_send(const struct bt_hci_transport *transport, struct net_buf *buffer)
{
 (void)transport; (void)buffer;
 fail_msg("A Controller-free buffer fixture unexpectedly transmitted HCI data");
 return -EIO;
}
static const struct bt_hci_driver_api driver_api = { .send = unexpected_send };
static const struct bt_hci_transport transport = {
 .name = "unopened-buffer-test", .api = &driver_api, .data = &driver_data,
};
void bt_test_select_unopened_hci(void)
{
 bt_dev.hci = &transport;
}
