# SPDX-License-Identifier: Apache-2.0

embt_library(embt_host_shell)
embt_library_sources(bt.c)
embt_library_sources_ifdef(CONFIG_BT_CONN gatt.c)
embt_library_sources_ifdef(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL l2cap.c)
embt_library_sources_ifdef(CONFIG_BT_ISO iso.c)
embt_library_sources_ifdef(CONFIG_BT_CHANNEL_SOUNDING cs.c)
