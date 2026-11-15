# SPDX-License-Identifier: Apache-2.0

embt_library(embt_services_ias_shell)
embt_library_sources_ifdef(CONFIG_BT_IAS ias.c)
embt_library_sources_ifdef(CONFIG_BT_IAS_CLIENT ias_client.c)
