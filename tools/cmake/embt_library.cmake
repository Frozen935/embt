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

function(embt_config_enabled symbol output)
  get_property(kind GLOBAL PROPERTY "EMBT_TYPE_${symbol}")
  if(NOT kind STREQUAL "bool" OR NOT DEFINED ${symbol})
    message(FATAL_ERROR "${symbol} is not a declared boolean Kconfig symbol")
  endif()
  set(${output} "${${symbol}}" PARENT_SCOPE)
endfunction()

function(_embt_current_target output)
  get_property(component DIRECTORY PROPERTY EMBT_COMPONENT)
  if(NOT component OR NOT TARGET "${component}")
    message(FATAL_ERROR "Declare embt_library(name) in ${CMAKE_CURRENT_SOURCE_DIR} first")
  endif()
  set(${output} "${component}" PARENT_SCOPE)
endfunction()

function(embt_library name)
  get_property(previous DIRECTORY PROPERTY EMBT_COMPONENT)
  if(previous OR TARGET "${name}")
    message(FATAL_ERROR "Duplicate EmBT component: ${name}")
  endif()
  add_library("${name}" OBJECT)
  set_target_properties("${name}" PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS YES)
  target_link_libraries("${name}" PRIVATE embt_config embt_internal)
  set_property(DIRECTORY PROPERTY EMBT_COMPONENT "${name}")
  set_property(GLOBAL APPEND PROPERTY EMBT_COMPONENTS "${name}")
endfunction()

function(embt_library_sources)
  _embt_current_target(component)
  foreach(source IN LISTS ARGN)
    get_filename_component(absolute "${source}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${absolute}" OR IS_DIRECTORY "${absolute}")
      message(FATAL_ERROR "Missing source for ${component}: ${absolute}")
    endif()
    get_filename_component(absolute "${absolute}" REALPATH)
    string(SHA256 key "${absolute}")
    get_property(owner GLOBAL PROPERTY "EMBT_SOURCE_${key}")
    if(owner AND NOT owner STREQUAL component)
      message(FATAL_ERROR "Source has two owners (${owner}, ${component}): ${absolute}")
    endif()
    if(NOT owner)
      target_sources("${component}" PRIVATE "${absolute}")
      set_property(GLOBAL PROPERTY "EMBT_SOURCE_${key}" "${component}")
    endif()
  endforeach()
endfunction()

function(embt_library_sources_ifdef symbol)
  embt_config_enabled("${symbol}" enabled)
  if(enabled)
    embt_library_sources(${ARGN})
  endif()
endfunction()

function(embt_add_subdirectory_ifdef symbol directory)
  embt_config_enabled("${symbol}" enabled)
  if(enabled)
    add_subdirectory("${directory}")
  endif()
endfunction()

function(embt_library_include_directories)
  _embt_current_target(component)
  target_include_directories("${component}" PRIVATE ${ARGN})
endfunction()

function(embt_library_include_directories_ifdef symbol)
  embt_config_enabled("${symbol}" enabled)
  if(enabled)
    embt_library_include_directories(${ARGN})
  endif()
endfunction()

function(embt_library_link_libraries)
  _embt_current_target(component)
  foreach(dependency IN LISTS ARGN)
    if(NOT TARGET "${dependency}")
      message(FATAL_ERROR "${component} needs a real dependency target: ${dependency}")
    endif()
  endforeach()
  target_link_libraries("${component}" PRIVATE ${ARGN})
endfunction()

function(embt_library_link_libraries_ifdef symbol)
  embt_config_enabled("${symbol}" enabled)
  if(enabled)
    embt_library_link_libraries(${ARGN})
  endif()
endfunction()

function(embt_finalize_library)
  get_property(components GLOBAL PROPERTY EMBT_COMPONENTS)
  if(NOT components)
    message(FATAL_ERROR "No EmBT source components are enabled")
  endif()
  foreach(component IN LISTS components)
    get_target_property(sources "${component}" SOURCES)
    if(NOT sources)
      message(FATAL_ERROR "Empty EmBT component: ${component}")
    endif()
    target_link_libraries(embt PRIVATE "${component}")
  endforeach()
  message(STATUS "EmBT components: ${components}")
endfunction()
