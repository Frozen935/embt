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

if(NOT EXISTS "${bt_test_executable}")
  message(FATAL_ERROR "Build ${bt_test_suite} before running CTest")
endif()
execute_process(COMMAND "${bt_test_executable}" --list-json
  RESULT_VARIABLE status OUTPUT_VARIABLE manifest ERROR_VARIABLE diagnostics)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Compiled case discovery failed: ${bt_test_suite}: ${diagnostics}")
endif()
file(READ "${bt_test_metadata}" metadata)
string(JSON execution GET "${metadata}" execution)
string(JSON timeout GET "${metadata}" timeout_s)
string(JSON group_count LENGTH "${manifest}" suites)
file(WRITE "${bt_test_working_directory}/${bt_test_suite}-cases.json" "${manifest}")
if(execution STREQUAL "suite")
  add_test("bluetooth.${bt_test_suite}.ordered_suite" "${bt_test_executable}")
  set_tests_properties("bluetooth.${bt_test_suite}.ordered_suite" PROPERTIES
    LABELS "bluetooth;unit" TIMEOUT ${timeout} WORKING_DIRECTORY "${bt_test_working_directory}")
  return()
endif()
if(group_count EQUAL 0)
  return()
endif()
math(EXPR last_group "${group_count} - 1")
foreach(group_index RANGE ${last_group})
  string(JSON suite GET "${manifest}" suites ${group_index} name)
  string(JSON case_count LENGTH "${manifest}" suites ${group_index} cases)
  if(case_count EQUAL 0)
    continue()
  endif()
  math(EXPR last_case "${case_count} - 1")
  foreach(case_index RANGE ${last_case})
    string(JSON case GET "${manifest}" suites ${group_index} cases ${case_index})
    string(JSON excluded ERROR_VARIABLE missing GET "${metadata}" excluded_cases "${case}")
    if(NOT missing)
      continue()
    endif()
    set(test_name "bluetooth.${bt_test_suite}.${case}")
    set(case_directory "${bt_test_working_directory}/case-state/${case}")
    file(REMOVE_RECURSE "${case_directory}")
    file(MAKE_DIRECTORY "${case_directory}")
    add_test("${test_name}" "${bt_test_executable}" --suite "${suite}" --case "${case}")
    set_tests_properties("${test_name}" PROPERTIES LABELS "bluetooth;unit" TIMEOUT ${timeout}
      SKIP_REGULAR_EXPRESSION "\\[  SKIPPED \\]|Could not run test: [^\n]*: Skipped!"
      WORKING_DIRECTORY "${case_directory}")
  endforeach()
endforeach()
