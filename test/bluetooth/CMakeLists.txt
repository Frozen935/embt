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

set(EMBT_BT_TEST_FILTER "core" CACHE STRING "Test unit directory or core")
set(BT_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(BT_TEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(core_suites addr uuid gap at bt_crypto bt_crypto_ccm hci_pkt host/monitor_buffer)
include("${EMBT_SOURCE_DIR}/tools/cmake/embt_bt_tests.cmake")

add_library(bt_test_support STATIC support/test_support.c)
target_link_libraries(bt_test_support PUBLIC
  embt_cmocka_framework embt_config embt_internal EmBT::osi embt_base)
target_compile_options(bt_test_support PRIVATE -ffunction-sections -fdata-sections)
if(TARGET EmBT::psa)
  target_link_libraries(bt_test_support PUBLIC EmBT::psa)
endif()
embt_test_instrument(bt_test_support)

file(GLOB_RECURSE descriptions CONFIGURE_DEPENDS RELATIVE "${BT_TEST_ROOT}"
  "${BT_TEST_ROOT}/*/test.json")
foreach(description IN LISTS descriptions)
  get_filename_component(name "${description}" DIRECTORY)
  if(EMBT_BT_TEST_FILTER STREQUAL "${name}" OR
     (EMBT_BT_TEST_FILTER STREQUAL "core" AND name IN_LIST core_suites))
    add_subdirectory("${name}")
  endif()
endforeach()
