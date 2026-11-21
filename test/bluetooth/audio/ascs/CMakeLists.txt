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

embt_bt_cmocka_begin(audio/ascs ISOLATED)

target_sources(test_bt_audio_ascs PRIVATE
  test_environment.c
  src/main.c
  src/test_ase_control_params.c
  src/test_ase_register.c
  src/test_ase_state_transition_invalid.c
  src/test_ase_state_transition.c
  src/test_common.c
  uut/bap_unicast_client.c
  uut/ascs.c
  uut/ascs_state.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/audio.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_iso.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_stream.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/addr.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/bt_str.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
  ${BT_TEST_ROOT}/audio/mocks/src/bap_stream.c
  ${BT_TEST_ROOT}/audio/mocks/src/conn.c
  ${BT_TEST_ROOT}/audio/mocks/src/gatt.c
  ${BT_TEST_ROOT}/audio/mocks/src/iso.c
  ${BT_TEST_ROOT}/audio/mocks/src/pacs.c
)

target_include_directories(test_bt_audio_ascs BEFORE PRIVATE "${BT_TEST_ROOT}/audio/mocks/include")

set(test_suites
  ascs_test_suite
  test_ase_control_params
  ascs_register_test_suite
  test_source_ase_state_transition
  test_sink_ase_state_transition
  test_ase_state_transition_invalid
)

embt_bt_cmocka_end(test_bt_audio_ascs ${test_suites})
