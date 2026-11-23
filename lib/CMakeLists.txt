# SPDX-License-Identifier: Apache-2.0

embt_add_subdirectory_ifdef(CONFIG_NET_BUF net_buf)
add_subdirectory(utils)
embt_add_subdirectory_ifdef(CONFIG_UUID uuid)

embt_add_subdirectory_ifdef(CONFIG_BT_SHELL shell)

embt_add_subdirectory_ifdef(CONFIG_CRC crc)
add_subdirectory(random)
embt_add_subdirectory_ifdef(CONFIG_SETTINGS settings)
