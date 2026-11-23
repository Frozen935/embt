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

embt_bt_cmocka_begin(buf)

target_sources(test_bt_buf PRIVATE
  src/main.c
  ${BT_TEST_ROOT}/support/unopened_hci.c
)

set(test_suites
  test_buf_data_api
)

embt_bt_cmocka_end(test_bt_buf ${test_suites})
