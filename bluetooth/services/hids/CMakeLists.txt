# SPDX-License-Identifier: Apache-2.0

embt_library(embt_services_hids)

embt_library_sources_ifdef(CONFIG_BT_HIDS hids.c)
