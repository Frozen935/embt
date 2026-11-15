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

add_custom_target(osi_tests)
set(manifest "")
foreach(module types time memory mutex sem cond thread timer io)
  set(source "${CMAKE_CURRENT_SOURCE_DIR}/test_osi_${module}.c")
  add_executable(test_osi_${module} "${source}" support/test_support.c)
  target_link_libraries(test_osi_${module} PRIVATE EmBT::osi EmBT::cmocka embt_config)
  target_compile_options(test_osi_${module} PRIVATE -Wall -Wextra -Werror)
  set_target_properties(test_osi_${module} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
  embt_test_instrument(test_osi_${module})
  add_dependencies(osi_tests test_osi_${module})
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
  file(STRINGS "${source}" cases REGEX "^static void test_[a-z_]+\\(void \\*\\*state\\)")
  if(NOT cases)
    message(FATAL_ERROR "No cases registered for OSI module ${module}")
  endif()
  foreach(declaration IN LISTS cases)
    string(REGEX REPLACE "^static void (test_[a-z_]+).*" "\\1" case "${declaration}")
    if(manifest)
      string(APPEND manifest ",\n")
    endif()
    string(APPEND manifest "  {\"module\": \"${module}\", \"case\": \"${case}\"}")
    add_test(NAME "osi.${module}.${case}" COMMAND test_osi_${module} "${case}")
    set_tests_properties("osi.${module}.${case}" PROPERTIES LABELS osi TIMEOUT 10)
  endforeach()
endforeach()

file(WRITE "${CMAKE_BINARY_DIR}/osi-test-manifest.json" "[\n${manifest}\n]\n")
