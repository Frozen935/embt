# SPDX-License-Identifier: Apache-2.0

if(CONFIG_PSA_CSPRNG_GENERATOR)
  embt_library(embt_random)
  embt_library_link_libraries(EmBT::psa)
  embt_library_sources(random_psa.c)
endif()
