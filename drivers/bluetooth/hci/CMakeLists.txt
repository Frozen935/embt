# SPDX-License-Identifier: Apache-2.0

if(CONFIG_EMBT_HCI_SOCKET)
  embt_library(embt_hci_socket)
  embt_library_sources(socket.c socket_io.c userchan.c userchan_bottom.c)
endif()
