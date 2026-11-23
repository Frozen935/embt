# SPDX-License-Identifier: Apache-2.0

add_subdirectory(common)
embt_add_subdirectory_ifdef(CONFIG_BT_HCI host)
embt_add_subdirectory_ifdef(CONFIG_BT_CONN services)
embt_add_subdirectory_ifdef(CONFIG_BT_MESH mesh)
embt_add_subdirectory_ifdef(CONFIG_BT_AUDIO audio)
embt_add_subdirectory_ifdef(CONFIG_BT_CRYPTO crypto)

add_subdirectory(lib)
