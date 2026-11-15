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

add_executable(btshell main.c runner.c)
target_link_libraries(btshell PRIVATE EmBT::embt embt_internal)
if(CONFIG_EMBT_AUDIO_SHELL)
  target_link_libraries(btshell PRIVATE EmBT::player)
endif()
set_target_properties(btshell PROPERTIES
  C_STANDARD 11 C_STANDARD_REQUIRED YES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
embt_test_instrument(btshell)
if(BUILD_TESTING)
  add_test(NAME btshell.cli
    COMMAND "${Python3_EXECUTABLE}" "${EMBT_SOURCE_DIR}/test/shell/smoke.py" "$<TARGET_FILE:btshell>")
  set_tests_properties(btshell.cli PROPERTIES LABELS btshell TIMEOUT 45)
endif()
