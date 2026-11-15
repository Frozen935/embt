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

if(TARGET EmBT::psa)
  message(FATAL_ERROR "Only one EmBT PSA provider can be integrated")
endif()

if(CONFIG_EMBT_CRYPTO_SYSTEM)
  find_package(MbedTLS CONFIG QUIET)
  if(TARGET MbedTLS::mbedcrypto)
    set(crypto_target MbedTLS::mbedcrypto)
  elseif(TARGET MbedTLS::tfpsacrypto)
    set(crypto_target MbedTLS::tfpsacrypto)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EMBT_SYSTEM_CRYPTO REQUIRED IMPORTED_TARGET GLOBAL mbedcrypto)
    set(crypto_target PkgConfig::EMBT_SYSTEM_CRYPTO)
  endif()
  set(integration SYSTEM)
elseif(CONFIG_EMBT_CRYPTO_EXTERNAL)
  if(NOT DEFINED EMBT_PSA_EXTERNAL_TARGET OR NOT TARGET "${EMBT_PSA_EXTERNAL_TARGET}")
    message(FATAL_ERROR "EXTERNAL needs an existing EMBT_PSA_EXTERNAL_TARGET with matching headers and library")
  endif()
  set(crypto_target "${EMBT_PSA_EXTERNAL_TARGET}")
  set(integration EXTERNAL)
elseif(CONFIG_EMBT_CRYPTO_SOURCE)
  if(NOT DEFINED EMBT_CRYPTO_SOURCE_DIR)
    set(EMBT_CRYPTO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/upstream")
  endif()
  get_filename_component(EMBT_CRYPTO_SOURCE_DIR "${EMBT_CRYPTO_SOURCE_DIR}" ABSOLUTE BASE_DIR "${EMBT_SOURCE_DIR}")
  if(NOT EXISTS "${EMBT_CRYPTO_SOURCE_DIR}/.embt-source.json")
    message(FATAL_ERROR "SOURCE needs the pinned release: run python3 tools/dependencies/prepare_mbedtls.py explicitly")
  endif()
  file(READ "${EMBT_SOURCE_DIR}/third_party/dependencies.lock.json" lock)
  file(READ "${EMBT_CRYPTO_SOURCE_DIR}/.embt-source.json" prepared)
  string(JSON expected_hash GET "${lock}" mbedtls archive_sha256)
  string(JSON actual_hash GET "${prepared}" archive_sha256)
  if(NOT expected_hash STREQUAL actual_hash)
    message(FATAL_ERROR "Prepared source does not match dependencies.lock.json")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${EMBT_SOURCE_DIR}/third_party/dependencies.lock.json" "${EMBT_CRYPTO_SOURCE_DIR}/.embt-source.json")
  set(provider_config "${CMAKE_BINARY_DIR}/generated/crypto")
  file(MAKE_DIRECTORY "${provider_config}")
  set(psa_definitions "")
  foreach(feature IN ITEMS
      ALG_CCM ALG_CMAC ALG_ECB_NO_PADDING ALG_ECDH ALG_HMAC ALG_MD5 ALG_SHA_1 ALG_SHA_256
      ECC_SECP_R1_256 KEY_TYPE_AES KEY_TYPE_ECC_KEY_PAIR_DERIVE KEY_TYPE_ECC_KEY_PAIR_EXPORT
      KEY_TYPE_ECC_KEY_PAIR_GENERATE KEY_TYPE_ECC_KEY_PAIR_IMPORT KEY_TYPE_HMAC)
    if(CONFIG_PSA_WANT_${feature})
      string(APPEND psa_definitions "#define PSA_WANT_${feature} 1\n")
    endif()
  endforeach()
  if(CONFIG_PSA_WANT_ECC_SECP_R1_256)
    string(APPEND psa_definitions "#define PSA_WANT_KEY_TYPE_ECC_KEY_PAIR_BASIC 1\n#define PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY 1\n")
  endif()
  set(MBEDTLS_CMAC_C ${CONFIG_PSA_WANT_ALG_CMAC})
  set(MBEDTLS_CCM_C ${CONFIG_PSA_WANT_ALG_CCM})
  set(MBEDTLS_ECDH_C ${CONFIG_PSA_WANT_ALG_ECDH})
  set(MBEDTLS_MD_C ${CONFIG_PSA_WANT_ALG_HMAC})
  set(MBEDTLS_MD5_C ${CONFIG_PSA_WANT_ALG_MD5})
  set(MBEDTLS_SHA1_C ${CONFIG_PSA_WANT_ALG_SHA_1})
  set(MBEDTLS_AES_ROM_TABLES ${CONFIG_MBEDTLS_AES_ROM_TABLES})
  set(MBEDTLS_AES_FEWER_TABLES ${CONFIG_MBEDTLS_AES_FEWER_TABLES})
  set(MBEDTLS_PSA_P256M_DRIVER_ENABLED ${CONFIG_MBEDTLS_PSA_P256M_DRIVER_ENABLED})
  set(MBEDTLS_SHA256_SMALLER ${CONFIG_MBEDTLS_SHA256_SMALLER})
  set(MBEDTLS_ECP_NIST_OPTIM ${CONFIG_MBEDTLS_ECP_NIST_OPTIM})
  configure_file(psa_config.h.in "${provider_config}/psa_config.h" @ONLY)
  configure_file(mbedtls_config.h.in "${provider_config}/mbedtls_config.h" @ONLY)
  set(MBEDTLS_CONFIG_FILE "${provider_config}/mbedtls_config.h")
  set(MBEDTLS_TARGET_PREFIX embt_vendor_)
  set(ENABLE_PROGRAMS OFF)
  set(ENABLE_TESTING OFF)
  set(USE_STATIC_MBEDTLS_LIBRARY ON)
  set(USE_SHARED_MBEDTLS_LIBRARY OFF)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
  add_subdirectory("${EMBT_CRYPTO_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/upstream" EXCLUDE_FROM_ALL)
  set(crypto_target embt_vendor_mbedcrypto)
  set(integration SOURCE)
else()
  message(FATAL_ERROR "PSA_CRYPTO requires one Mbed TLS integration mode")
endif()

add_library(embt_psa_provider INTERFACE)
add_library(EmBT::psa ALIAS embt_psa_provider)
target_link_libraries(embt_psa_provider INTERFACE "${crypto_target}")

set(EMBT_CHECK_AES ${CONFIG_PSA_WANT_KEY_TYPE_AES})
set(EMBT_CHECK_ECB ${CONFIG_PSA_WANT_ALG_ECB_NO_PADDING})
set(EMBT_CHECK_CMAC ${CONFIG_PSA_WANT_ALG_CMAC})
set(EMBT_CHECK_CCM ${CONFIG_PSA_WANT_ALG_CCM})
set(EMBT_CHECK_ECDH ${CONFIG_PSA_WANT_ALG_ECDH})
set(EMBT_CHECK_HMAC ${CONFIG_PSA_WANT_ALG_HMAC})
set(EMBT_CHECK_SHA1 ${CONFIG_PSA_WANT_ALG_SHA_1})
set(EMBT_CHECK_SHA256 ${CONFIG_PSA_WANT_ALG_SHA_256})
set(EMBT_CHECK_MD5 ${CONFIG_PSA_WANT_ALG_MD5})
set(EMBT_CHECK_HELPERS ${CONFIG_EMBT_MBEDTLS_HELPERS})
configure_file(requirements.h.in "${CMAKE_CURRENT_BINARY_DIR}/requirements.h")
add_executable(embt_crypto_probe EXCLUDE_FROM_ALL validate.c)
target_include_directories(embt_crypto_probe PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(embt_crypto_probe PRIVATE EmBT::psa)

if(CMAKE_CROSSCOMPILING)
  add_custom_target(embt_crypto_check
    COMMAND "${CMAKE_COMMAND}" -E echo "PSA compile/link checked; run embt_crypto_probe on the target to validate algorithms and entropy"
    DEPENDS embt_crypto_probe VERBATIM)
else()
  add_custom_target(embt_crypto_check
    COMMAND "$<TARGET_FILE:embt_crypto_probe>" DEPENDS embt_crypto_probe VERBATIM)
endif()
message(STATUS "EmBT PSA: ${integration}, target=${crypto_target}; one Mbed TLS implementation")
