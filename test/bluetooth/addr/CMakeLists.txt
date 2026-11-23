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

embt_bt_cmocka_begin(addr)

target_sources(test_bt_addr PRIVATE
  src/test_bt_addr_le_eq.c
  src/test_bt_addr_from_str.c
  src/test_bt_addr_le_is_resolved.c
  src/test_bt_addr_le_from_str.c
  src/test_bt_addr_le_to_str.c
  src/test_bt_addr_matches_irk.c
)

set(test_suites
  bt_addr_from_str
  bt_addr_le_eq
  bt_addr_le_from_str
  bt_addr_le_is_resolved
  bt_addr_le_to_str
  bt_addr_matches_irk
)

if(CONFIG_BT_SMP AND CONFIG_BT_PRIVACY)
  target_sources(test_bt_addr PRIVATE src/test_bt_addr_le_resolve.c)
  list(INSERT test_suites 4 bt_addr_le_rpa_resolve)
endif()

embt_bt_cmocka_end(test_bt_addr ${test_suites})
