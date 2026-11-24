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


if(CONFIG_EMBT_STACK)
  if(CONFIG_BT)
    message(FATAL_ERROR "Disable zblue CONFIG_BT before enabling EmBT")
  endif()
  if(CONFIG_PSA_CRYPTO AND NOT EXISTS "${NUTTX_APPS_DIR}/crypto/mbedtls/mbedtls/include/psa/crypto.h")
    message(FATAL_ERROR "Prepare platform Mbed TLS explicitly before enabling EmBT")
  endif()
  set(EMBT_OSI_SANITIZER NONE CACHE STRING "Project instrumentation")
  set(EMBT_PSA_EXTERNAL_TARGET mbedtls)
  if(TARGET mbedtls)
    target_include_directories(mbedtls INTERFACE
      "${NUTTX_APPS_DIR}/crypto/mbedtls/include"
      "${NUTTX_APPS_DIR}/crypto/mbedtls/mbedtls/include")
  endif()
  add_subdirectory(source)
  get_property(components GLOBAL PROPERTY EMBT_COMPONENTS)
  foreach(component IN LISTS components)
    nuttx_add_library_internal(${component})
  endforeach()
  nuttx_add_external_library(embt)
  if(CONFIG_EMBT_BTSHELL)
    nuttx_add_application(NAME btshell
      PRIORITY ${CONFIG_EMBT_BTSHELL_PRIORITY}
      STACKSIZE ${CONFIG_EMBT_BTSHELL_STACKSIZE}
      SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/apps/btshell/main.c"
           "${CMAKE_CURRENT_SOURCE_DIR}/source/apps/btshell/runner.c"
      DEPENDS embt)
    target_link_libraries(apps_btshell PRIVATE EmBT::embt embt_internal)
  endif()
  if(CONFIG_EMBT_PORT_TESTS)
    foreach(module types time memory mutex sem cond thread timer io)
      nuttx_add_application(NAME embt_osi_${module} PRIORITY 100 STACKSIZE 16384
        SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/test/osdep/test_osi_${module}.c"
             "${CMAKE_CURRENT_SOURCE_DIR}/source/test/osdep/support/test_support.c"
        DEPENDS embt_osi cmocka embt_config)
      target_compile_options(apps_embt_osi_${module} PRIVATE -Wall -Wextra -Werror)
    endforeach()
    nuttx_add_application(NAME embt_settings_check PRIORITY 100 STACKSIZE 16384
      SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/test/platform/nuttx_settings.c"
      DEPENDS embt)
    target_link_libraries(apps_embt_settings_check PRIVATE EmBT::embt)
    nuttx_add_application(NAME embt_work_semantics PRIORITY 100 STACKSIZE 16384
      SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/test/bringup/test_work_semantics.c"
      DEPENDS embt cmocka)
    target_link_libraries(apps_embt_work_semantics PRIVATE EmBT::embt embt_internal)
    target_link_options(apps_embt_work_semantics PRIVATE
      "-Wl,--wrap=osi_cond_init" "-Wl,--wrap=osi_cond_wait")
  endif()
endif()
