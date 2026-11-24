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
if(NOT EMBT_COMPILER_NUTTX)
  find_package(Threads REQUIRED)
  set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
endif()
if(NOT EMBT_COMPILER_NUTTX)
  set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  set(CMAKE_REQUIRED_LIBRARIES Threads::Threads rt)
  check_symbol_exists(pthread_mutex_clocklock "pthread.h" EMBT_HAVE_MUTEX_CLOCKLOCK)
  if(NOT EMBT_HAVE_MUTEX_CLOCKLOCK)
    message(FATAL_ERROR "Linux POSIX requires pthread_mutex_clocklock")
  endif()
endif()
check_symbol_exists(sem_clockwait "semaphore.h" EMBT_HAVE_SEM_CLOCKWAIT)
check_symbol_exists(pthread_condattr_setclock "pthread.h" EMBT_HAVE_COND_CLOCK)
check_symbol_exists(clock_nanosleep "time.h" EMBT_HAVE_CLOCK_NANOSLEEP)
check_symbol_exists(timer_create "time.h;signal.h" EMBT_HAVE_TIMER_CREATE)
check_symbol_exists(SIGEV_THREAD "signal.h" EMBT_HAVE_SIGEV_THREAD)
if(NOT EMBT_HAVE_SEM_CLOCKWAIT OR NOT EMBT_HAVE_COND_CLOCK OR
   NOT EMBT_HAVE_CLOCK_NANOSLEEP OR NOT EMBT_HAVE_TIMER_CREATE OR NOT EMBT_HAVE_SIGEV_THREAD)
  message(FATAL_ERROR "Shared POSIX requires monotonic semaphore/condition waits, sleep and timers")
endif()
unset(CMAKE_REQUIRED_DEFINITIONS)
unset(CMAKE_REQUIRED_LIBRARIES)
embt_library(embt_osi)
add_library(EmBT::osi ALIAS embt_osi)
embt_library_sources(sync.c thread.c thread_config.c timer.c time.c memory.c io.c)
target_include_directories(embt_osi PUBLIC
  "${EMBT_SOURCE_DIR}/include")
if(NOT EMBT_COMPILER_NUTTX)
  target_link_libraries(embt_osi PUBLIC Threads::Threads)
  target_compile_definitions(embt_osi PUBLIC _GNU_SOURCE)
  target_link_libraries(embt_osi PUBLIC rt)
endif()
