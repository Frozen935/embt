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

embt_bt_cmocka_begin(audio/cap_handover ISOLATED)

target_sources(test_bt_audio_cap_handover PRIVATE
  test_environment.c
  src/broadcast_to_unicast.c
  src/callbacks.c
  src/cancel.c
  src/unicast_to_broadcast.c
  src/test_common.c
  uut/bap_broadcast_assistant.c
  uut/cap_handover.c
  uut/cap_initiator.c
  uut/csip.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/audio.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/codec.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_broadcast_source.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_iso.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_stream.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_commander.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_handover.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_initiator.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_common.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/cap_stream.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/ccid.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/addr.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/bt_str.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
  ${BT_TEST_ROOT}/audio/mocks/src/adv.c
  ${BT_TEST_ROOT}/audio/mocks/src/bap_unicast_client.c
  ${BT_TEST_ROOT}/audio/mocks/src/conn.c
  ${BT_TEST_ROOT}/audio/mocks/src/gatt.c
  ${BT_TEST_ROOT}/audio/mocks/src/iso.c
)

target_include_directories(test_bt_audio_cap_handover BEFORE PRIVATE "${BT_TEST_ROOT}/audio/mocks/include")

set(test_suites
  cap_handover_broadcast_to_unicast_test_suite
  cap_handover_callbacks_test_suite
  cap_handover_test_cancel
  cap_handover_unicast_to_broadcast_test_suite
)

embt_bt_cmocka_end(test_bt_audio_cap_handover ${test_suites})
