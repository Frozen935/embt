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

embt_bt_cmocka_begin(uuid)

target_sources(test_bt_uuid PRIVATE
  src/test_bt_uuid_cmp.c
  src/test_bt_uuid_create.c
  src/test_bt_uuid_from_str.c
  src/test_bt_uuid_to_str.c
)

set(test_suites
  bt_uuid_cmp
  bt_uuid_create
  bt_uuid_from_str
  bt_uuid_to_str
)

embt_bt_cmocka_end(test_bt_uuid ${test_suites})
