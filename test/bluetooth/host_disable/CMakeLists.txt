# SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
# SPDX-License-Identifier: Apache-2.0

# /****************************************************************************
#  *  Copyright (C) 2026 EmBT contributors
#  *
#  * Licensed under the Apache License, Version 2.0 (the "License");
#  * you may not use this file except in compliance with the License.
#  * You may obtain a copy of the License at
#  *
#  *      http://www.apache.org/licenses/LICENSE-2.0
#  *
#  * Unless required by applicable law or agreed to in writing, software
#  * distributed under the License is distributed on an "AS IS" BASIS,
#  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  * See the License for the specific language governing permissions and
#  * limitations under the License.
#  ***************************************************************************/
#

embt_bt_cmocka_begin(host_disable)

target_sources(test_bt_host_disable PRIVATE
  src/main.c
  ${BT_TEST_ROOT}/support/assert.c
)

target_link_options(test_bt_host_disable PRIVATE
  "-Wl,--wrap=bt_assert_fail"
)

set(test_suites
  bt_send_blocks
  bt_ctlr_stops
  bt_disable
)

if(CONFIG_BT_PERIPHERAL AND CONFIG_BT_GATT_CLIENT)
  list(PREPEND test_suites bt_host_ncp bt_conn_tx)
endif()

embt_bt_cmocka_end(test_bt_host_disable ${test_suites})
