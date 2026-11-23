# SPDX-License-Identifier: Apache-2.0

embt_library(embt_net_buf)
embt_library_sources_ifdef(
  CONFIG_NET_BUF
  buf.c
  buf_simple.c
  )
