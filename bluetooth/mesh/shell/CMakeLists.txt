# SPDX-License-Identifier: Apache-2.0

embt_library(embt_mesh_shell)

embt_library_sources(
  shell.c
  utils.c
)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_HEALTH_CLI health.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_CFG_CLI cfg.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_LARGE_COMP_DATA_CLI large_comp_data.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_OP_AGG_CLI op_agg.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_PRIV_BEACON_CLI priv_beacon.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_RPR_CLI rpr.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_SAR_CFG_CLI sar.c)

if(CONFIG_BT_MESH_SHELL_BLOB_CLI OR CONFIG_BT_MESH_SHELL_BLOB_SRV)
  embt_library_sources(blob.c)
endif()

if(CONFIG_BT_MESH_SHELL_DFU_SLOT OR CONFIG_BT_MESH_SHELL_DFU_METADATA OR
  CONFIG_BT_MESH_SHELL_DFU_CLI OR CONFIG_BT_MESH_SHELL_DFU_SRV)
  embt_library_sources(dfu.c)
endif()

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_DFD_SRV dfd.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OD_PRIV_PROXY_CLI od_priv_proxy.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SOL_PDU_RPL_CLI sol_pdu_rpl.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SHELL_BRG_CFG_CLI brg_cfg.c)
