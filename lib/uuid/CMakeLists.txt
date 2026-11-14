# SPDX-License-Identifier: Apache-2.0

embt_library(embt_uuid)
embt_library_link_libraries_ifdef(CONFIG_UUID_V5 EmBT::psa)

embt_library_sources_ifdef(CONFIG_UUID uuid.c)
