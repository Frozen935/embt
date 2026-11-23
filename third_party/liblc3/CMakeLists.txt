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

include(FetchContent)
set(EMBT_LIBLC3_SOURCE_DIR "" CACHE PATH "Existing pinned liblc3 source directory")
if(EMBT_LIBLC3_SOURCE_DIR)
  set(lc3_source "${EMBT_LIBLC3_SOURCE_DIR}")
else()
  FetchContent_Declare(embt_lc3_source
    URL https://github.com/zephyrproject-rtos/liblc3/archive/48bbd3eacd36e99a57317a0a4867002e0b09e183.tar.gz
    URL_HASH SHA256=1d3be59a24938865ca710a4d69c183bd3948fe91ed2db2e82188d3c62d7e809c)
  FetchContent_GetProperties(embt_lc3_source)
  if(NOT embt_lc3_source_POPULATED)
    FetchContent_Populate(embt_lc3_source)
  endif()
  set(lc3_source "${embt_lc3_source_SOURCE_DIR}")
endif()
if(NOT EXISTS "${lc3_source}/include/lc3.h")
  message(FATAL_ERROR "Selected liblc3 source is missing its public interface")
endif()
file(GLOB lc3_sources CONFIGURE_DEPENDS "${lc3_source}/src/*.c")
add_library(embt_lc3_provider STATIC ${lc3_sources})
target_include_directories(embt_lc3_provider PUBLIC "${lc3_source}/include")
target_link_libraries(embt_lc3_provider PUBLIC m)
add_library(EmBT::lc3 ALIAS embt_lc3_provider)
