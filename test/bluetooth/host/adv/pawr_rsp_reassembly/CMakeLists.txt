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

embt_bt_cmocka_begin(host/adv/pawr_rsp_reassembly ISOLATED)

target_sources(test_bt_host_adv_pawr_rsp_reassembly PRIVATE
  src/main.c
  src/stubs.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/adv.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/addr.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/bt_str.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
)

set(test_suites
  pawr_rsp_reassembly
)

embt_bt_cmocka_end(test_bt_host_adv_pawr_rsp_reassembly ${test_suites})
