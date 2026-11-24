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

function(embt_generate_external_kconfig)
  if(EXISTS "${NUTTX_APPS_BINDIR}/Kconfig")
    return()
  endif()

  set(external_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  string(REPLACE "/" "_" menu_prefix "${external_dir}")
  if(WIN32)
    string(REPLACE ":" "_" menu_prefix "${menu_prefix}")
  endif()
  set(menu_file "${NUTTX_APPS_BINDIR}/${menu_prefix}_Kconfig")
  file(WRITE "${menu_file}" "menu \"External\"\n")
  file(GLOB entries LIST_DIRECTORIES false "${external_dir}/*/CMakeLists.txt")
  foreach(entry IN LISTS entries)
    get_filename_component(directory "${entry}" DIRECTORY)
    get_filename_component(name "${directory}" NAME)
    if((NUTTX_CONFIG STREQUAL "embt" AND name STREQUAL "zblue") OR
       (NOT NUTTX_CONFIG STREQUAL "embt" AND name STREQUAL "embt"))
      continue()
    endif()
    set(source_menu "${directory}/Kconfig")
    string(REPLACE "/" "_" generated_name "${source_menu}")
    if(WIN32)
      string(REPLACE ":" "_" generated_name "${generated_name}")
    endif()
    # Preserve the platform's recursive generated menus when present.
    if(EXISTS "${NUTTX_APPS_BINDIR}/${generated_name}")
      file(APPEND "${menu_file}"
        "source \"${NUTTX_APPS_BINDIR}/${generated_name}\"\n")
    elseif(EXISTS "${source_menu}")
      file(APPEND "${menu_file}" "source \"${source_menu}\"\n")
    endif()
  endforeach()
  file(APPEND "${menu_file}" "endmenu # External\n")
endfunction()
