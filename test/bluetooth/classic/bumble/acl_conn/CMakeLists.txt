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

add_executable(test_bt_classic_acl integration_main.c src/main.c)
target_include_directories(test_bt_classic_acl PRIVATE "${BT_TEST_ROOT}")
target_link_libraries(test_bt_classic_acl PRIVATE bt_test_support EmBT::embt)
embt_test_instrument(test_bt_classic_acl)
set_target_properties(test_bt_classic_acl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BT_TEST_BINARY_DIR}/classic/bumble")

target_link_options(test_bt_classic_acl PRIVATE "-Wl,--wrap=bt_base_require")
