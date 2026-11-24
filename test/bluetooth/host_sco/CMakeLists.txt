# SPDX-FileCopyrightText: Copyright 2026 NXP
# SPDX-License-Identifier: Apache-2.0

embt_bt_cmocka_begin(host_sco)
target_sources(test_bt_host_sco PRIVATE src/main.c)
target_link_options(test_bt_host_sco PRIVATE
  "-Wl,--wrap=bt_conn_add_sco"
  "-Wl,--wrap=bt_conn_lookup_addr_sco"
  "-Wl,--wrap=bt_conn_set_state"
  "-Wl,--wrap=bt_conn_unref"
  "-Wl,--wrap=bt_hci_cmd_send_sync"
  "-Wl,--wrap=bt_sco_cleanup")
embt_bt_cmocka_end(test_bt_host_sco sco_hooks)
