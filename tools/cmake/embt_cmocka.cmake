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

function(embt_cmocka_registry target)
  set(BT_TEST_REGISTRY_DECLARATIONS "")
  set(BT_TEST_REGISTRY_ENTRIES "")
  set(seen "")
  foreach(suite IN LISTS ARGN)
    if(NOT suite MATCHES "^[A-Za-z_][A-Za-z0-9_]*$" OR suite IN_LIST seen)
      message(FATAL_ERROR "Invalid or duplicate suite identifier: ${suite}")
    endif()
    list(APPEND seen "${suite}")
    string(APPEND BT_TEST_REGISTRY_DECLARATIONS
      "extern const struct bt_test_suite bt_test_suite_${suite};\n")
    string(APPEND BT_TEST_REGISTRY_ENTRIES "\t&bt_test_suite_${suite},\n")
  endforeach()
  set(registry "${CMAKE_CURRENT_BINARY_DIR}/${target}_registry.c")
  configure_file("${EMBT_SOURCE_DIR}/test/framework/registry.c.in" "${registry}" @ONLY)
  target_sources(${target} PRIVATE "${registry}")
endfunction()
