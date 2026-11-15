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
 * @brief Public-header consumer used with whole-archive linking.
 */

#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/gatt.h>
#include <embt/bluetooth/l2cap.h>
#include <embt/bluetooth/transport.h>
#if defined(CONFIG_BT_MESH)
#include <embt/bluetooth/mesh.h>
#endif
#if defined(CONFIG_BT_AUDIO)
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/pacs.h>
#endif

/** @brief Link-only consumer; deliberately does not start the Bluetooth stack. */
int main(void)
{
	return 0;
}
