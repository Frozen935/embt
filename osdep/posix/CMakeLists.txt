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

include(CheckSymbolExists)
find_package(Threads REQUIRED)
set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(pthread_mutex_clocklock "pthread.h" EMBT_HAVE_MUTEX_CLOCKLOCK)
check_symbol_exists(sem_clockwait "semaphore.h" EMBT_HAVE_SEM_CLOCKWAIT)
if(NOT EMBT_HAVE_MUTEX_CLOCKLOCK OR NOT EMBT_HAVE_SEM_CLOCKWAIT)
  message(FATAL_ERROR "The POSIX backend requires monotonic mutex/semaphore waits from the target libc")
endif()
embt_library(embt_osi)
add_library(EmBT::osi ALIAS embt_osi)
embt_library_sources(sync.c thread.c timer.c time.c memory.c io.c)
target_include_directories(embt_osi PUBLIC
  "${EMBT_SOURCE_DIR}/osdep/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(embt_osi PUBLIC _GNU_SOURCE)
target_link_libraries(embt_osi PUBLIC Threads::Threads rt)
if(BUILD_TESTING)
  embt_test_instrument(embt_osi)
endif()
