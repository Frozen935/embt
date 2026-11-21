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

embt_bt_cmocka_begin(audio/cap_commander ISOLATED)

target_sources(test_bt_audio_cap_commander PRIVATE
  test_environment.c
  src/main.c
  src/test_common.c
  src/test_vcp.c
  src/test_micp.c
  src/test_broadcast_reception.c
  src/test_distribute_broadcast_code.c
  src/test_cancel.c
  uut/bap_broadcast_assistant.c
  uut/aics.c
  uut/cap_commander.c
  uut/csip.c
  uut/micp.c
  uut/vcp.c
  uut/vocs.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/audio.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_commander.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_common.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/addr.c
  ${BT_TEST_ROOT}/audio/mocks/src/conn.c
  ${BT_TEST_ROOT}/audio/mocks/src/gatt.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
)

target_include_directories(test_bt_audio_cap_commander BEFORE PRIVATE "${BT_TEST_ROOT}/audio/mocks/include")

set(test_suites
  cap_commander_test_suite
  cap_commander_test_broadcast_reception
  cap_commander_test_cancel
  cap_commander_test_distribute_broadcast_code
  cap_commander_test_micp
  cap_commander_test_vcp
)

embt_bt_cmocka_end(test_bt_audio_cap_commander ${test_suites})
