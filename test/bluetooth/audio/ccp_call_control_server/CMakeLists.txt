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

embt_bt_cmocka_begin(audio/ccp_call_control_server ISOLATED)

target_sources(test_bt_audio_ccp_call_control_server PRIVATE
  test_environment.c
  src/main.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/audio.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/ccid.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/ccp_call_control_server.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/tbs.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/bt_str.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
  ${BT_TEST_ROOT}/audio/mocks/src/conn.c
  ${BT_TEST_ROOT}/audio/mocks/src/gatt.c
)

target_include_directories(test_bt_audio_ccp_call_control_server BEFORE PRIVATE "${BT_TEST_ROOT}/audio/mocks/include")

set(test_suites
  ccp_call_control_server_test_suite
)

embt_bt_cmocka_end(test_bt_audio_ccp_call_control_server ${test_suites})
