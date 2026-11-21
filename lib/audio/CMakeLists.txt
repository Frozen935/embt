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

add_library(embt_player STATIC)
if(CONFIG_EMBT_A2DP_PLAYER)
  target_sources(embt_player PRIVATE a2dp_player.c)
endif()
if(CONFIG_EMBT_BAP_PLAYER)
  target_sources(embt_player PRIVATE bap_player.c)
endif()
add_library(EmBT::player ALIAS embt_player)
target_link_libraries(embt_player PUBLIC EmBT::pcm PRIVATE EmBT::embt embt_internal)
target_include_directories(embt_player PUBLIC "${EMBT_SOURCE_DIR}/include")
embt_test_instrument(embt_player)
