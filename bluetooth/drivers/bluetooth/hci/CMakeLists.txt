# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_USERCHAN)
  embt_library(embt_hci_userchan)
  embt_library_include_directories(../../..)
  embt_library_sources(userchan.c userchan_bottom.c)
endif()
