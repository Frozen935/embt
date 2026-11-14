# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_EAD OR CONFIG_LIBSBC)
  embt_library(embt_bt_lib)

  embt_library_sources_ifdef(CONFIG_BT_EAD ead.c)
  embt_library_sources_ifdef(CONFIG_LIBSBC sbc.c)
endif()
