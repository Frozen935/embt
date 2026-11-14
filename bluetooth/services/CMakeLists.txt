# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_ANS OR CONFIG_BT_DIS OR CONFIG_BT_GAP_SVC OR CONFIG_BT_CTS OR
   CONFIG_BT_ETS OR CONFIG_BT_HRS OR CONFIG_BT_TPS)
  embt_library(embt_services)

  embt_library_sources_ifdef(CONFIG_BT_ANS ans.c)

  embt_library_sources_ifdef(CONFIG_BT_DIS dis.c)

  embt_library_sources_ifdef(CONFIG_BT_GAP_SVC gap_svc.c)

  embt_library_sources_ifdef(CONFIG_BT_CTS cts.c)

  embt_library_sources_ifdef(CONFIG_BT_ETS ets.c)

  embt_library_sources_ifdef(CONFIG_BT_HRS hrs.c)

  embt_library_sources_ifdef(CONFIG_BT_TPS tps.c)
endif()

if(CONFIG_BT_BAS)
  add_subdirectory(bas)
endif()

if(CONFIG_BT_OTS OR CONFIG_BT_OTS_CLIENT)
  add_subdirectory(ots)
endif()

if(CONFIG_BT_IAS OR CONFIG_BT_IAS_CLIENT)
  add_subdirectory(ias)
endif()

if(CONFIG_BT_ZEPHYR_NUS)
  add_subdirectory(nus)
endif()

if(CONFIG_BT_HIDS)
  add_subdirectory(hids)
endif()
