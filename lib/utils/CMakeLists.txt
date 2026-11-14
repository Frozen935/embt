# SPDX-License-Identifier: Apache-2.0

zephyr_sources(
  dec.c
  hex.c
)

zephyr_sources_ifdef(CONFIG_BASE64 base64.c)
zephyr_sources_ifdef(CONFIG_BT_MESH bitmap.c)
zephyr_sources_ifdef(CONFIG_UTF8 utf8.c)
