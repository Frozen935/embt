# SPDX-License-Identifier: Apache-2.0

if(CONFIG_PSA_CSPRNG_GENERATOR)
  zephyr_library()
  zephyr_library_sources(random_psa.c)
endif()
