# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_USERCHAN)
  zephyr_library_sources(userchan.c userchan_bottom.c)
endif()
