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

embt_bt_cmocka_begin(hci_pkt ISOLATED)

target_sources(test_bt_hci_pkt PRIVATE
  src/main.c
  src/lockstep.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/hci_pkt.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/hci_lockstep.c
)

set(test_suites
  bt_hci_lockstep
  bt_hci_pkt
)

embt_bt_cmocka_end(test_bt_hci_pkt ${test_suites})
