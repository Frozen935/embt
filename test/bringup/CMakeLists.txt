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
add_executable(test_init_order test_init_order.c)
target_link_libraries(test_init_order PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_compile_options(test_init_order PRIVATE -Werror)
embt_test_instrument(test_init_order)
add_test(NAME bringup.base.initialization COMMAND test_init_order)
set_tests_properties(bringup.base.initialization PROPERTIES LABELS bringup TIMEOUT 10)
target_link_libraries(test_base PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_compile_options(test_base PRIVATE -Werror)
embt_test_instrument(test_base)
foreach(case atomic queue queue_cancel semaphore slab netbuf work work_cancel
    work_lifetime work_async_cancel deadline_lifetime pool_reuse registries time_model)
  add_test(NAME "bringup.base.${case}" COMMAND test_base "${case}")
  set_tests_properties("bringup.base.${case}" PROPERTIES LABELS bringup TIMEOUT 10)
endforeach()

add_executable(test_log test_log.c log_module_debug.c log_module_declare.c
  log_module_warn.c log_module_off.c)
target_link_libraries(test_log PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_compile_options(test_log PRIVATE -Werror)
embt_test_instrument(test_log)
add_test(NAME bringup.base.logging COMMAND test_log)
set_tests_properties(bringup.base.logging PROPERTIES LABELS bringup TIMEOUT 10)

add_executable(test_compiler test_compiler.c)
target_link_libraries(test_compiler PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_compile_options(test_compiler PRIVATE -Werror)
set_target_properties(test_compiler PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
embt_test_instrument(test_compiler)
add_test(NAME bringup.base.compiler COMMAND test_compiler)
set_tests_properties(bringup.base.compiler PROPERTIES LABELS bringup TIMEOUT 10)

add_executable(test_queue_memory test_queue_memory.c)
target_link_libraries(test_queue_memory PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_link_options(test_queue_memory PRIVATE "-Wl,--wrap=osi_sem_init"
  "-Wl,--wrap=osi_malloc" "-Wl,--wrap=osi_calloc" "-Wl,--wrap=osi_aligned_alloc" "-Wl,--wrap=osi_free")
target_compile_options(test_queue_memory PRIVATE -Werror)
embt_test_instrument(test_queue_memory)
add_test(NAME bringup.base.queue_memory COMMAND test_queue_memory)
set_tests_properties(bringup.base.queue_memory PROPERTIES LABELS bringup TIMEOUT 15)

add_executable(test_work_lifecycle test_work_lifecycle.c)
target_link_libraries(test_work_lifecycle PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_link_options(test_work_lifecycle PRIVATE "-Wl,--wrap=osi_thread_create"
  "-Wl,--wrap=osi_sem_init" "-Wl,--wrap=osi_calloc")
target_compile_options(test_work_lifecycle PRIVATE -Werror)
embt_test_instrument(test_work_lifecycle)
add_test(NAME bringup.base.work_lifecycle COMMAND test_work_lifecycle)
set_tests_properties(bringup.base.work_lifecycle PROPERTIES LABELS bringup TIMEOUT 15)

add_executable(test_work_semantics test_work_semantics.c)
target_link_libraries(test_work_semantics PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_link_options(test_work_semantics PRIVATE "-Wl,--wrap=osi_cond_init" "-Wl,--wrap=osi_cond_wait")
target_compile_options(test_work_semantics PRIVATE -Werror)
embt_test_instrument(test_work_semantics)
add_test(NAME bringup.base.work_semantics COMMAND test_work_semantics)
set_tests_properties(bringup.base.work_semantics PROPERTIES LABELS bringup TIMEOUT 15)

add_executable(test_diagnostics test_diagnostics.c)
target_link_libraries(test_diagnostics PRIVATE EmBT::embt EmBT::cmocka embt_internal)
target_link_options(test_diagnostics PRIVATE "-Wl,--wrap=osi_vprint" "-Wl,--wrap=osi_malloc")
target_compile_options(test_diagnostics PRIVATE -Werror)
embt_test_instrument(test_diagnostics)
add_test(NAME bringup.base.diagnostics COMMAND test_diagnostics)
set_tests_properties(bringup.base.diagnostics PROPERTIES LABELS bringup TIMEOUT 15)

foreach(variant normal ndebug)
  add_executable(test_public_assert_${variant} test_public_assert.c)
  target_link_libraries(test_public_assert_${variant} PRIVATE EmBT::embt EmBT::cmocka)
  target_compile_options(test_public_assert_${variant} PRIVATE -Wall -Wextra -Werror)
  if(variant STREQUAL "ndebug")
    target_compile_definitions(test_public_assert_${variant} PRIVATE NDEBUG)
  endif()
  embt_test_instrument(test_public_assert_${variant})
  add_test(NAME bringup.assert.public_${variant} COMMAND test_public_assert_${variant})
  set_tests_properties(bringup.assert.public_${variant} PROPERTIES LABELS bringup TIMEOUT 10)
endforeach()
