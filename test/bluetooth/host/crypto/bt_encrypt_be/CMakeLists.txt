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

embt_bt_cmocka_begin(host/crypto/bt_encrypt_be ISOLATED)

target_sources(test_bt_host_crypto_bt_encrypt_be PRIVATE
  src/main.c
  src/test_suite_invalid_inputs.c
  ${BT_TEST_ROOT}/host/crypto/mocks/aes.c
  ${BT_TEST_ROOT}/host/crypto/mocks/aes_expects.c
  ${BT_TEST_ROOT}/host/crypto/mocks/prng.c
  ${BT_TEST_ROOT}/host/crypto/mocks/prng_expects.c
  ${BT_TEST_ROOT}/host/crypto/mocks/hci_core.c
  ${BT_TEST_ROOT}/host/crypto/mocks/hci_core_expects.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/crypto_psa.c
  ${EMBT_SOURCE_DIR}/bluetooth/common/bt_str.c
  ${EMBT_SOURCE_DIR}/bluetooth/host/uuid.c
  ${BT_TEST_ROOT}/support/assert.c
)

target_link_options(test_bt_host_crypto_bt_encrypt_be PRIVATE
  "-Wl,--wrap=bt_assert_fail"
)

set(test_suites
  bt_encrypt_be
  bt_encrypt_be_invalid_cases
)

embt_bt_cmocka_end(test_bt_host_crypto_bt_encrypt_be ${test_suites})
