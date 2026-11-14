# SPDX-License-Identifier: Apache-2.0

embt_library(embt_crypto)
embt_library_link_libraries_ifdef(CONFIG_PSA_CRYPTO EmBT::psa)

embt_library_sources(bt_crypto.c)

embt_library_sources(bt_crypto_psa.c)
