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
set(EMBT_OSI_SANITIZER "ADDRESS" CACHE STRING "Project instrumentation: NONE, ADDRESS, THREAD")
set_property(CACHE EMBT_OSI_SANITIZER PROPERTY STRINGS NONE ADDRESS THREAD)
if(NOT EMBT_OSI_SANITIZER MATCHES "^(NONE|ADDRESS|THREAD)$")
  message(FATAL_ERROR "Unknown EMBT_OSI_SANITIZER=${EMBT_OSI_SANITIZER}")
endif()

function(embt_test_instrument target)
  if(EMBT_OSI_SANITIZER STREQUAL "ADDRESS")
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
  elseif(EMBT_OSI_SANITIZER STREQUAL "THREAD")
    target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=thread)
  endif()
endfunction()
