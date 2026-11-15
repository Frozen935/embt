# SPDX-License-Identifier: Apache-2.0

embt_library(embt_services_ias)

embt_add_subdirectory_ifdef(CONFIG_BT_SHELL shell)

embt_library_sources_ifdef(
        CONFIG_BT_IAS
        ias.c)

embt_library_sources_ifdef(
        CONFIG_BT_IAS_CLIENT
        ias_client.c)
