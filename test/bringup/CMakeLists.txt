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

add_executable(test_base test_base.c)
target_link_libraries(test_base PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_compile_options(test_base PRIVATE -Werror)
embt_test_instrument(test_base)
foreach(case atomic queue queue_cancel semaphore slab arena netbuf work work_cancel work_lifetime work_async_cancel deadline_lifetime pool_reuse registries)
  add_test(NAME "bringup.base.${case}" COMMAND test_base "${case}")
  set_tests_properties("bringup.base.${case}" PROPERTIES LABELS bringup TIMEOUT 10)
endforeach()

add_executable(test_log test_log.c)
target_link_libraries(test_log PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_compile_options(test_log PRIVATE -Werror)
embt_test_instrument(test_log)
add_test(NAME bringup.base.logging COMMAND test_log)
set_tests_properties(bringup.base.logging PROPERTIES LABELS bringup TIMEOUT 10)

add_executable(test_transport test_transport.c)
target_include_directories(test_transport PRIVATE "${EMBT_SOURCE_DIR}/drivers/bluetooth/hci")
target_link_libraries(test_transport PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_link_options(test_transport PRIVATE "-Wl,--wrap=socket" "-Wl,--wrap=bind" "-Wl,--wrap=ioctl")
target_compile_options(test_transport PRIVATE -Werror)
embt_test_instrument(test_transport)
foreach(case tcp unix user_model close_exhausted bad_packet open_failure device_down_failure stream_backpressure)
  add_test(NAME "bringup.transport.${case}" COMMAND test_transport "${case}")
  set_tests_properties("bringup.transport.${case}" PROPERTIES LABELS bringup TIMEOUT 10)
endforeach()
