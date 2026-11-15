# Copyright (c) 2024 Croxel, Inc.
# SPDX-License-Identifier: Apache-2.0

embt_library(embt_services_nus)

embt_library_include_directories(.)
embt_library_sources(
  nus.c
  nus_inst.c
)

embt_library_sources_ifdef(CONFIG_BT_ZEPHYR_NUS_AUTO_START_BLUETOOTH
  bt_nus_auto_start_bt.c
)
