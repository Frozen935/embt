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
set(EMBT_LIBSBC_SOURCE_DIR "" CACHE PATH "Existing pinned libsbc source directory")
if(EMBT_LIBSBC_SOURCE_DIR)
  set(sbc_source "${EMBT_LIBSBC_SOURCE_DIR}")
else()
  FetchContent_Declare(embt_libsbc_source
    URL https://github.com/zephyrproject-rtos/libsbc/archive/8e1beda02acb8972e29e6edbb423f7cafe16e445.tar.gz
    URL_HASH SHA256=dfa4152490f9762fd1d2edfc9136a0cfc7eef1a00175414ccb9ec24076a23e45)
  FetchContent_GetProperties(embt_libsbc_source)
  if(NOT embt_libsbc_source_POPULATED)
    FetchContent_Populate(embt_libsbc_source)
  endif()
  set(sbc_source "${embt_libsbc_source_SOURCE_DIR}")
endif()
if(NOT EXISTS "${sbc_source}/encoder/include/sbc_encoder.h")
  message(FATAL_ERROR "Selected libsbc source is missing its encoder interface")
endif()
file(GLOB encoder_sources CONFIGURE_DEPENDS "${sbc_source}/encoder/srce/*.c")
file(GLOB decoder_sources CONFIGURE_DEPENDS "${sbc_source}/decoder/srce/*.c")
add_library(embt_sbc_provider STATIC ${encoder_sources} ${decoder_sources})
target_include_directories(embt_sbc_provider PUBLIC
  "${sbc_source}/encoder/include" "${sbc_source}/decoder/include")
add_library(EmBT::sbc ALIAS embt_sbc_provider)
