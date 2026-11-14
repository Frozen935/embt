# SPDX-License-Identifier: Apache-2.0

embt_add_subdirectory_ifdef(CONFIG_CRC crc)
add_subdirectory(random)
embt_add_subdirectory_ifdef(CONFIG_SETTINGS settings)
