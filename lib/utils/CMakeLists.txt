# SPDX-License-Identifier: Apache-2.0

embt_library(embt_utils)

embt_library_sources(
  dec.c
  hex.c
)

embt_library_sources_ifdef(CONFIG_BASE64 base64.c)
embt_library_sources_ifdef(CONFIG_BT_MESH bitmap.c)
embt_library_sources_ifdef(CONFIG_UTF8 utf8.c)
