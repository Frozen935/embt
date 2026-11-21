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

include("${EMBT_SOURCE_DIR}/tools/cmake/embt_cmocka.cmake")
add_library(embt_cmocka_framework STATIC runner.c assertions.c)
target_include_directories(embt_cmocka_framework PUBLIC "${EMBT_SOURCE_DIR}/test")
target_link_libraries(embt_cmocka_framework PUBLIC EmBT::cmocka EmBT::osi)
target_compile_options(embt_cmocka_framework PRIVATE -Wall -Wextra -Werror)
set_target_properties(embt_cmocka_framework PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_link_options(embt_cmocka_framework INTERFACE
  "-Wl,--wrap=_assert_true" "-Wl,--wrap=_assert_int_equal"
  "-Wl,--wrap=_assert_int_not_equal" "-Wl,--wrap=_assert_memory_equal"
  "-Wl,--wrap=_assert_string_equal" "-Wl,--wrap=_fail")
embt_test_instrument(embt_cmocka_framework)

if(EMBT_BT_TEST_FILTER STREQUAL "framework")
  add_executable(test_bt_framework_probe main.c probe.c)
  target_link_libraries(test_bt_framework_probe PRIVATE embt_cmocka_framework)
  embt_cmocka_registry(test_bt_framework_probe positive negative)
  embt_test_instrument(test_bt_framework_probe)
  add_executable(test_bt_framework_missing EXCLUDE_FROM_ALL main.c)
  target_link_libraries(test_bt_framework_missing PRIVATE embt_cmocka_framework)
  embt_cmocka_registry(test_bt_framework_missing missing_descriptor)
  embt_test_instrument(test_bt_framework_missing)
  add_test(NAME framework.contract
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_framework.py"
      "$<TARGET_FILE:test_bt_framework_probe>" "${CMAKE_BINARY_DIR}")
  set_tests_properties(framework.contract PROPERTIES LABELS framework TIMEOUT 20)
  add_test(NAME framework.variant_runner
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_variant_runner.py"
      "${EMBT_SOURCE_DIR}/tools/tests/run_bluetooth_tests.py")
  set_tests_properties(framework.variant_runner PROPERTIES LABELS framework TIMEOUT 20)
  add_test(NAME framework.classic_responses
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_classic_responses.py")
  set_tests_properties(framework.classic_responses PROPERTIES LABELS framework TIMEOUT 20)
  add_test(NAME framework.platform_config
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_platform_config.py")
  set_tests_properties(framework.platform_config PROPERTIES LABELS framework TIMEOUT 20)
endif()
