# SPDX-License-Identifier: Apache-2.0

if(CONFIG_EMBT_HCI_SOCKET)
  embt_library(embt_hci_socket)
  embt_library_sources(socket.c socket_io.c)
  if(NOT EMBT_COMPILER_NUTTX)
    embt_library_sources(userchan.c userchan_bottom.c)
  endif()
endif()

embt_add_subdirectory_ifdef(CONFIG_EMBT_HCI_H4 h4)
