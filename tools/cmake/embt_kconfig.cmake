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

include_guard(GLOBAL)

function(embt_configure)
  find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
  set(driver "${EMBT_SOURCE_DIR}/tools/kconfig/configure.py")
  set(build "${CMAKE_CURRENT_BINARY_DIR}")
  set(arguments --build-dir "${build}")
  if(DEFINED EMBT_DEFCONFIG AND NOT EMBT_DEFCONFIG STREQUAL "")
    list(APPEND arguments --defconfig "${EMBT_DEFCONFIG}")
  endif()
  if(DEFINED EMBT_CONFIG_FRAGMENTS)
    foreach(fragment IN LISTS EMBT_CONFIG_FRAGMENTS)
      list(APPEND arguments --fragment "${fragment}")
    endforeach()
  endif()
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${driver}" configure ${arguments}
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "EmBT configuration failed (${result})")
  endif()

  set(config_json "${build}/kconfig/config.json")
  file(READ "${config_json}" configuration)
  string(JSON schema GET "${configuration}" schema_version)
  if(NOT schema EQUAL 1)
    message(FATAL_ERROR "Unsupported EmBT configuration schema: ${schema}")
  endif()
  string(JSON count LENGTH "${configuration}" symbols)
  math(EXPR last "${count} - 1")
  foreach(index RANGE ${last})
    string(JSON symbol MEMBER "${configuration}" symbols ${index})
    string(JSON kind GET "${configuration}" symbols "${symbol}" type)
    string(JSON value GET "${configuration}" symbols "${symbol}" value)
    set(${symbol} "${value}")
    set(${symbol} "${value}" PARENT_SCOPE)
    set_property(GLOBAL PROPERTY "EMBT_TYPE_${symbol}" "${kind}")
  endforeach()

  string(JSON count LENGTH "${configuration}" dependencies)
  math(EXPR last "${count} - 1")
  foreach(index RANGE ${last})
    string(JSON dependency GET "${configuration}" dependencies ${index})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${dependency}")
  endforeach()
  set(generated "${build}/generated/include")
  set(header "${generated}/embt/autoconf.h")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${build}/.config" "${header}" "${generated}/zephyr/autoconf.h"
    "${config_json}" "${build}/kconfig/dependencies.json")

  # Validate all retained component conditions, even in disabled directories.
  file(GLOB_RECURSE component_files CONFIGURE_DEPENDS
    "${EMBT_SOURCE_DIR}/bluetooth/CMakeLists.txt" "${EMBT_SOURCE_DIR}/lib/CMakeLists.txt")
  foreach(input IN LISTS component_files)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    file(READ "${input}" content)
    string(REGEX MATCHALL "CONFIG_[A-Z0-9_]+" references "${content}")
    foreach(symbol IN LISTS references)
      get_property(kind GLOBAL PROPERTY "EMBT_TYPE_${symbol}")
      if(NOT kind STREQUAL "bool")
        message(FATAL_ERROR "Unknown/non-boolean condition ${symbol} in ${input}")
      endif()
    endforeach()
  endforeach()

  add_library(embt_config INTERFACE)
  target_include_directories(embt_config INTERFACE "${generated}")
  target_compile_options(embt_config INTERFACE
    "$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include \"${header}\">")

  foreach(action menuconfig olddefconfig)
    add_custom_target(${action}
      COMMAND "${Python3_EXECUTABLE}" "${driver}" "${action}" --build-dir "${build}"
      WORKING_DIRECTORY "${EMBT_SOURCE_DIR}"
      USES_TERMINAL VERBATIM)
  endforeach()
  add_custom_target(savedefconfig
    COMMAND "${Python3_EXECUTABLE}" "${driver}" savedefconfig --build-dir "${build}"
      --output "${build}/defconfig" --force
    WORKING_DIRECTORY "${EMBT_SOURCE_DIR}" USES_TERMINAL VERBATIM)
endfunction()
