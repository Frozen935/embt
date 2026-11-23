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

add_executable(test_audio_pcm test_pcm.c fake.c)
target_include_directories(test_audio_pcm PRIVATE "${EMBT_SOURCE_DIR}/drivers/audio")
target_link_libraries(test_audio_pcm PRIVATE EmBT::pcm EmBT::cmocka embt_internal)
target_compile_options(test_audio_pcm PRIVATE -Werror)
embt_test_instrument(test_audio_pcm)
add_test(NAME audio.pcm COMMAND test_audio_pcm)
set_tests_properties(audio.pcm PROPERTIES LABELS audio TIMEOUT 15)
if(CONFIG_EMBT_AUDIO_PULSE)
  add_executable(test_audio_pulse test_pulse.c)
  target_link_libraries(test_audio_pulse PRIVATE EmBT::pcm EmBT::cmocka embt_internal)
  target_compile_options(test_audio_pulse PRIVATE -Werror)
  embt_test_instrument(test_audio_pulse)
endif()
if(CONFIG_EMBT_A2DP_PLAYER)
  add_executable(test_audio_sbc test_sbc.c fake.c)
  target_include_directories(test_audio_sbc PRIVATE "${EMBT_SOURCE_DIR}/drivers/audio")
  target_link_libraries(test_audio_sbc PRIVATE EmBT::player EmBT::cmocka embt_internal)
  target_compile_options(test_audio_sbc PRIVATE -Werror)
  embt_test_instrument(test_audio_sbc)
  add_test(NAME audio.sbc COMMAND test_audio_sbc)
  set_tests_properties(audio.sbc PROPERTIES LABELS audio TIMEOUT 15)
endif()
if(CONFIG_EMBT_BAP_PLAYER)
  add_executable(test_audio_lc3 test_lc3.c fake.c)
  target_include_directories(test_audio_lc3 PRIVATE "${EMBT_SOURCE_DIR}/drivers/audio")
  target_link_libraries(test_audio_lc3 PRIVATE EmBT::player EmBT::cmocka embt_internal)
  target_compile_options(test_audio_lc3 PRIVATE -Werror)
  embt_test_instrument(test_audio_lc3)
  add_test(NAME audio.lc3 COMMAND test_audio_lc3)
  set_tests_properties(audio.lc3 PROPERTIES LABELS audio TIMEOUT 15)
endif()
