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

cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build/posix -G Ninja")
endif()
project(EmBT LANGUAGES C)
set(EMBT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_cmake_property(cache_variables CACHE_VARIABLES)
foreach(variable IN LISTS cache_variables)
  if(variable MATCHES "^CONFIG_")
    message(FATAL_ERROR "${variable} must come from Kconfig, not the CMake cache")
  endif()
endforeach()

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
  message(FATAL_ERROR "The initial EmBT build supports the Ninja generator")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  message(FATAL_ERROR "The initial EmBT build supports Linux with GCC or Clang")
endif()
if(CMAKE_BUILD_TYPE OR CMAKE_C_FLAGS MATCHES "(^|[ \t])-O")
  message(FATAL_ERROR "Select optimization through menuconfig; leave CMAKE_BUILD_TYPE and global -O flags unset")
endif()

include("${EMBT_SOURCE_DIR}/tools/cmake/embt_library.cmake")
include("${EMBT_SOURCE_DIR}/tools/cmake/embt_kconfig.cmake")
include("${EMBT_SOURCE_DIR}/tools/cmake/embt_dependencies.cmake")
embt_configure()
embt_config_enabled(CONFIG_EMBT_STACK stack_enabled)
if(NOT stack_enabled)
  message(STATUS "EmBT stack disabled; configuration targets remain available")
  return()
endif()
embt_config_enabled(CONFIG_EMBT stack_enabled)
if(NOT stack_enabled)
  message(FATAL_ERROR "CONFIG_EMBT is disabled; enable it with mkbt.sh menuconfig")
endif()
if(CONFIG_EMBT_64BIT)
  set(expected_pointer_size 8)
else()
  set(expected_pointer_size 4)
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL expected_pointer_size)
  message(FATAL_ERROR "CONFIG_EMBT_64BIT does not match the compiler pointer ABI")
endif()
include(CheckCSourceCompiles)
if(CONFIG_EMBT_BIG_ENDIAN)
  set(byte_order BIG_ENDIAN)
else()
  set(byte_order LITTLE_ENDIAN)
endif()
check_c_source_compiles(
  "#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_${byte_order}__\n#error Wrong target byte order\n#endif\nint main(void) { return 0; }"
  "EMBT_COMPILER_${byte_order}")
if(NOT EMBT_COMPILER_${byte_order})
  message(FATAL_ERROR "Selected EmBT byte order does not match the compiler target")
endif()

add_library(embt_internal INTERFACE)
target_include_directories(embt_internal INTERFACE "${EMBT_SOURCE_DIR}/include" "${EMBT_SOURCE_DIR}/bluetooth")
target_compile_options(embt_internal INTERFACE -Wall -Wextra)
if(CONFIG_NO_OPTIMIZATIONS)
  target_compile_options(embt_internal INTERFACE -O0)
elseif(CONFIG_SIZE_OPTIMIZATIONS)
  target_compile_options(embt_internal INTERFACE -Os)
else()
  target_compile_options(embt_internal INTERFACE -O2)
endif()

add_library(embt STATIC)
add_library(EmBT::embt ALIAS embt)
target_include_directories(embt PUBLIC "${EMBT_SOURCE_DIR}/include")
target_link_libraries(embt PUBLIC embt_config)
set_property(GLOBAL PROPERTY EMBT_COMPONENTS "")
embt_configure_dependencies()
add_subdirectory(lib)
add_subdirectory(bluetooth/subsys)
add_subdirectory(bluetooth/drivers)
add_subdirectory(bluetooth)
embt_finalize_library()
