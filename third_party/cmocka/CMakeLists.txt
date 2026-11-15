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

set(EMBT_CMOCKA_PROVIDER "SYSTEM" CACHE STRING "cmocka provider: SYSTEM or SOURCE")
set_property(CACHE EMBT_CMOCKA_PROVIDER PROPERTY STRINGS SYSTEM SOURCE)
if(EMBT_CMOCKA_PROVIDER STREQUAL "SYSTEM")
  find_package(cmocka 1.1.7 EXACT CONFIG REQUIRED)
  set_property(TARGET cmocka::cmocka PROPERTY IMPORTED_GLOBAL TRUE)
  add_library(EmBT::cmocka ALIAS cmocka::cmocka)
  message(STATUS "OSI tests use system cmocka 1.1.7")
elseif(EMBT_CMOCKA_PROVIDER STREQUAL "SOURCE")
  file(READ "${EMBT_SOURCE_DIR}/third_party/dependencies.lock.json" lock)
  string(JSON source_path GET "${lock}" cmocka source_subdirectory)
  set(source "${EMBT_SOURCE_DIR}/${source_path}")
  if(NOT EXISTS "${source}/.embt-source.json")
    message(FATAL_ERROR "Prepare pinned cmocka explicitly with tools/dependencies/prepare_cmocka.py")
  endif()
  file(READ "${source}/.embt-source.json" marker)
  foreach(field version archive_sha256 archive_url archive_root license source_subdirectory)
    string(JSON actual GET "${marker}" "${field}")
    string(JSON required GET "${lock}" cmocka "${field}")
    if(NOT actual STREQUAL required)
      message(FATAL_ERROR "cmocka source marker does not match the dependency lock")
    endif()
  endforeach()
  set(UNIT_TESTING OFF CACHE BOOL "cmocka upstream tests" FORCE)
  set(WITH_EXAMPLES OFF CACHE BOOL "cmocka upstream examples" FORCE)
  add_subdirectory("${source}" "${CMAKE_CURRENT_BINARY_DIR}/upstream" EXCLUDE_FROM_ALL)
  add_library(EmBT::cmocka ALIAS cmocka)
  message(STATUS "OSI tests use pinned cmocka source 1.1.7")
else()
  message(FATAL_ERROR "Unknown cmocka provider: ${EMBT_CMOCKA_PROVIDER}")
endif()
