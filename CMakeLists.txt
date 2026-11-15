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
option(BUILD_TESTING "Build EmBT OSI tests" OFF)
set(EMBT_BUILD_SCOPE "FULL" CACHE STRING "Build scope: FULL or OSI")
set_property(CACHE EMBT_BUILD_SCOPE PROPERTY STRINGS FULL OSI)
if(NOT EMBT_BUILD_SCOPE MATCHES "^(FULL|OSI)$")
  message(FATAL_ERROR "Unknown EmBT build scope: ${EMBT_BUILD_SCOPE}")
endif()
if(EMBT_BUILD_SCOPE STREQUAL "OSI" AND NOT BUILD_TESTING)
  message(FATAL_ERROR "OSI scope requires BUILD_TESTING=ON")
endif()
include("${EMBT_SOURCE_DIR}/tools/cmake/embt_testing.cmake")

get_cmake_property(cache_variables CACHE_VARIABLES)
foreach(variable IN LISTS cache_variables)
  if(variable MATCHES "^CONFIG_")
    message(FATAL_ERROR "${variable} must come from Kconfig, not the CMake cache")
  endif()
endforeach()

if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
  message(FATAL_ERROR "The initial EmBT build supports the Ninja generator")
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  message(FATAL_ERROR "The shared POSIX build requires GCC or Clang")
endif()
if(CMAKE_BUILD_TYPE OR CMAKE_C_FLAGS MATCHES "(^|[ \t])-O")
  message(FATAL_ERROR "Select optimization through menuconfig; leave CMAKE_BUILD_TYPE and global -O flags unset")
endif()

include("${EMBT_SOURCE_DIR}/tools/cmake/embt_library.cmake")
include("${EMBT_SOURCE_DIR}/tools/cmake/embt_kconfig.cmake")
include("${EMBT_SOURCE_DIR}/tools/cmake/embt_dependencies.cmake")
if(BUILD_TESTING AND EMBT_BUILD_SCOPE STREQUAL "FULL" AND
   (NOT DEFINED EMBT_BT_TEST_FILTER OR EMBT_BT_TEST_FILTER STREQUAL "core"))
  list(REMOVE_ITEM EMBT_CONFIG_FRAGMENTS "${EMBT_SOURCE_DIR}/test/bluetooth/core.conf")
  list(PREPEND EMBT_CONFIG_FRAGMENTS "${EMBT_SOURCE_DIR}/test/bluetooth/core.conf")
endif()
embt_configure()
embt_config_enabled(CONFIG_EMBT_STACK stack_enabled)
if(NOT stack_enabled)
  if(BUILD_TESTING)
    message(FATAL_ERROR "OSI tests require CONFIG_EMBT_STACK=y")
  endif()
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
include(CheckSymbolExists)
check_symbol_exists(__NuttX__ "pthread.h" EMBT_COMPILER_NUTTX)
if(NOT EMBT_COMPILER_NUTTX AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The POSIX backend supports Linux and native NuttX targets")
endif()
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
target_compile_options(embt_internal INTERFACE -Wall -Wextra -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
if(CONFIG_NO_OPTIMIZATIONS)
  target_compile_options(embt_internal INTERFACE -O0)
elseif(CONFIG_SIZE_OPTIMIZATIONS)
  target_compile_options(embt_internal INTERFACE -Os)
else()
  target_compile_options(embt_internal INTERFACE -O2)
endif()

target_include_directories(embt_internal INTERFACE "${EMBT_SOURCE_DIR}/lib/shell/include")

set_property(GLOBAL PROPERTY EMBT_COMPONENTS "")
add_subdirectory(osdep)
if(BUILD_TESTING)
  include(CTest)
  add_subdirectory(third_party/cmocka)
  add_subdirectory(test)
endif()
if(EMBT_BUILD_SCOPE STREQUAL "OSI")
  return()
endif()

add_library(embt STATIC)
add_library(EmBT::embt ALIAS embt)
target_include_directories(embt PUBLIC "${EMBT_SOURCE_DIR}/include")
target_link_libraries(embt PUBLIC embt_config)
embt_configure_dependencies()
add_subdirectory(base)
add_subdirectory(lib)
add_subdirectory(drivers)
add_subdirectory(bluetooth)
embt_finalize_library()

# Whole-archive linking detects missing providers hidden by a static archive build.
add_executable(embt_link_check test/compile/link_check.c)
target_link_libraries(embt_link_check PRIVATE
  "-Wl,--whole-archive" EmBT::embt "-Wl,--no-whole-archive")
embt_test_instrument(embt_link_check)

if(BUILD_TESTING)
  add_subdirectory(test/bringup)
  add_subdirectory(test/bluetooth)
endif()

embt_add_subdirectory_ifdef(CONFIG_BT_SHELL apps/btshell)
