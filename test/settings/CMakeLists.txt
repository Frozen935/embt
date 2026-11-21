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

add_executable(test_settings test_settings.c)
target_link_libraries(test_settings PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_link_options(test_settings PRIVATE "-Wl,--wrap=write" "-Wl,--wrap=rename" "-Wl,--wrap=fsync" "-Wl,--wrap=read")
target_compile_options(test_settings PRIVATE -Wall -Wextra -Werror)
embt_test_instrument(test_settings)
foreach(case crud handlers direct callback_reentry read_bounds compression binary_limits partial_tail corruption write_failures read_failure rename_failure sync_failure modes_symlink init_retry restart process_writers bluetooth_identity)
  add_test(NAME "settings.file.${case}" COMMAND test_settings "${case}")
  set_tests_properties("settings.file.${case}" PROPERTIES LABELS "settings;platform;filesystem" TIMEOUT 20)
endforeach()
