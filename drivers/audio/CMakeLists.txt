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

add_library(embt_pcm STATIC audio.c)
add_library(EmBT::pcm ALIAS embt_pcm)
target_link_libraries(embt_pcm PUBLIC embt_config PRIVATE EmBT::osi embt_internal)
target_include_directories(embt_pcm PUBLIC "${EMBT_SOURCE_DIR}/include")
embt_test_instrument(embt_pcm)
if(CONFIG_EMBT_AUDIO_PULSE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(EMBT_PULSE REQUIRED IMPORTED_TARGET libpulse)
  target_sources(embt_pcm PRIVATE pulse/pulse.c)
  target_link_libraries(embt_pcm PRIVATE PkgConfig::EMBT_PULSE)
endif()
