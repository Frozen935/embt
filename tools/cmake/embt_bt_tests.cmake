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

include_guard(GLOBAL)

function(embt_bt_cmocka_begin name)
  cmake_parse_arguments(TEST "ISOLATED" "" "" ${ARGN})
  string(REPLACE "/" "_" token "${name}")
  set(target "test_bt_${token}")
  add_executable(${target} "${EMBT_SOURCE_DIR}/test/framework/main.c")
  target_link_libraries(${target} PRIVATE bt_test_support)
  target_link_options(${target} PRIVATE "-Wl,--wrap=bt_base_require")
  target_include_directories(${target} PRIVATE
    "${BT_TEST_ROOT}" "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${BT_TEST_ROOT}/host" "${BT_TEST_ROOT}/host/crypto"
    "${EMBT_SOURCE_DIR}/bluetooth/crypto" "${EMBT_SOURCE_DIR}/bluetooth/host"
    "${EMBT_SOURCE_DIR}/third_party/fff")
  if(TEST_ISOLATED)
    target_link_libraries(${target} PRIVATE embt_utils embt_uuid embt_net_buf)
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${target} PRIVATE -Wl,--gc-sections)
  else()
    target_link_libraries(${target} PRIVATE EmBT::embt)
  endif()
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BT_TEST_BINARY_DIR}"
    BT_TEST_UNIT_NAME "${name}")
  embt_test_instrument(${target})
endfunction()

function(embt_bt_cmocka_end target)
  embt_cmocka_registry(${target} ${ARGN})
  get_target_property(name ${target} BT_TEST_UNIT_NAME)
  set(discovery "${CMAKE_CURRENT_BINARY_DIR}/${target}_discover.cmake")
  file(GENERATE OUTPUT "${discovery}" CONTENT
    "set(bt_test_executable \"$<TARGET_FILE:${target}>\")\nset(bt_test_suite \"${name}\")\nset(bt_test_metadata \"${CMAKE_CURRENT_SOURCE_DIR}/test.json\")\nset(bt_test_working_directory \"${BT_TEST_BINARY_DIR}\")\ninclude(\"${BT_TEST_ROOT}/support/discover.cmake\")\n")
  set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${discovery}")
endfunction()

function(embt_bt_shell_test name)
  string(REPLACE "/" "_" token "${name}")
  set(target "test_bt_${token}")
  add_executable(${target} ${ARGN} "${EMBT_SOURCE_DIR}/apps/btshell/runner.c")
  target_include_directories(${target} PRIVATE "${BT_TEST_ROOT}" "${EMBT_SOURCE_DIR}/bluetooth"
    "${EMBT_SOURCE_DIR}/apps/btshell")
  target_link_libraries(${target} PRIVATE bt_test_support EmBT::embt)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${BT_TEST_BINARY_DIR}/classic")
  embt_test_instrument(${target})
endfunction()
