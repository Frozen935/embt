# SPDX-License-Identifier: Apache-2.0

embt_library(embt_common)

embt_library_sources(addr.c)
embt_library_sources(dummy.c)
embt_library_sources(bt_str.c)
embt_library_sources(hci_pkt.c)
embt_library_sources(hci_lockstep.c)

embt_library_sources_ifdef(CONFIG_BT_HCI_SET_PUBLIC_ADDR hci_public_addr.c)
embt_library_sources_ifdef(CONFIG_BT_RPA   rpa.c)
embt_library_sources_ifdef(CONFIG_BT_LONG_WQ long_wq.c)
embt_library_sources_ifdef(CONFIG_BT_PRIVATE_SHELL bt_shell_private.c)
