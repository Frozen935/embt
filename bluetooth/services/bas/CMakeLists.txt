# SPDX-License-Identifier: Apache-2.0

embt_library(embt_services_bas)
embt_library_sources_ifdef(CONFIG_BT_BAS bas.c)
embt_library_sources_ifdef(CONFIG_BT_BAS_BLS bas_bls.c)
embt_library_sources_ifdef(CONFIG_BT_BAS_BCS bas_bcs.c)
