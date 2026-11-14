# SPDX-License-Identifier: Apache-2.0

embt_library(embt_services_ots)

embt_library_sources_ifdef(
  CONFIG_BT_OTS
  ots.c
  ots_l2cap.c
  ots_obj_manager.c
  ots_oacp.c
  ots_olcp.c)

  embt_library_sources_ifdef(CONFIG_BT_OTS_DIR_LIST_OBJ ots_dir_list.c)

embt_library_sources_ifdef(
  CONFIG_BT_OTS_CLIENT
  ots_client.c
  ots_l2cap.c)
