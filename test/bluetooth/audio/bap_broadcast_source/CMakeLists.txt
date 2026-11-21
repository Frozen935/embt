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

embt_bt_cmocka_begin(audio/bap_broadcast_source ISOLATED)

target_sources(test_bt_audio_bap_broadcast_source PRIVATE
  src/main.c
  src/test_callback_register.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/audio.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_iso.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_stream.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/bap_broadcast_source.c
  ${EMBT_SOURCE_DIR}/bluetooth/audio/codec.c
  ${BT_TEST_ROOT}/audio/mocks/src/bap_broadcast_source.c
  ${BT_TEST_ROOT}/audio/mocks/src/bap_stream.c
  ${BT_TEST_ROOT}/audio/mocks/src/conn.c
  ${BT_TEST_ROOT}/audio/mocks/src/iso.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
)

target_include_directories(test_bt_audio_bap_broadcast_source BEFORE PRIVATE "${BT_TEST_ROOT}/audio/mocks/include")

set(test_suites
  bap_broadcast_source_test_suite
  bap_broadcast_source_test_cb_register_suite
)

embt_bt_cmocka_end(test_bt_audio_bap_broadcast_source ${test_suites})
