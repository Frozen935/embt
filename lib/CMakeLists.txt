# SPDX-License-Identifier: Apache-2.0

embt_add_subdirectory_ifdef(CONFIG_NET_BUF net_buf)
add_subdirectory(utils)
embt_add_subdirectory_ifdef(CONFIG_UUID uuid)
