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

# Use one provider for both PSA operations and any Mbed TLS helper needs.
function(embt_configure_dependencies)
  if(CONFIG_PSA_CRYPTO)
    add_subdirectory("${EMBT_SOURCE_DIR}/third_party/mbedtls"
      "${CMAKE_CURRENT_BINARY_DIR}/third_party/mbedtls")
    target_link_libraries(embt PRIVATE EmBT::psa)
    add_dependencies(embt embt_crypto_check)
  endif()
  foreach(symbol CONFIG_LIBSBC CONFIG_LIBLC3)
    embt_config_enabled("${symbol}" required)
    if(required)
      message(STATUS "${symbol}=y: codec provider not integrated")
    endif()
  endforeach()
  message(STATUS "POSIX runtime and HCI user-channel porting are still pending")
endfunction()
