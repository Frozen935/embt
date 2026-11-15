# SPDX-License-Identifier: Apache-2.0

embt_library(embt_mesh)
embt_library_link_libraries_ifdef(CONFIG_PSA_CRYPTO EmBT::psa)

embt_library_sources_ifdef(CONFIG_BT_MESH
    main.c
    cfg.c
    adv.c
    beacon.c
    net.c
    subnet.c
    app_keys.c
    heartbeat.c
    crypto.c
    crypto_psa.c
    access.c
    msg.c
    cfg_srv.c
    health_srv.c
    va.c
    transport.c
)

embt_library_sources_ifdef(CONFIG_BT_MESH_ADV_LEGACY adv_legacy.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_ADV_EXT adv_ext.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PENDING_STORE settings.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_RPL_STORAGE_MODE_SETTINGS rpl.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_LOW_POWER lpn.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_FRIEND friend.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PROV prov.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PROVISIONEE provisionee.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PROVISIONER provisioner.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PB_ADV pb_adv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PB_GATT_COMMON pb_gatt.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PB_GATT pb_gatt_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PB_GATT_CLIENT pb_gatt_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_GATT_CLIENT gatt_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PROXY_CLIENT proxy_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_GATT_PROXY proxy_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_GATT proxy_msg.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_CFG_CLI cfg_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_HEALTH_CLI health_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SAR_CFG_CLI sar_cfg_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SAR_CFG_SRV sar_cfg_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SAR_CFG  sar_cfg.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OP_AGG_SRV  op_agg_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OP_AGG_CLI  op_agg_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OP_AGG op_agg.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_LARGE_COMP_DATA_SRV  large_comp_data_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_LARGE_COMP_DATA_CLI  large_comp_data_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PRIV_BEACON_SRV priv_beacon_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_PRIV_BEACON_CLI priv_beacon_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SELF_TEST test.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_CDB cdb.c)

embt_add_subdirectory_ifdef(CONFIG_BT_MESH_SHELL shell)

embt_library_sources_ifdef(CONFIG_BT_MESH_BLOB_SRV blob_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_BLOB_CLI blob_cli.c)


embt_library_sources_ifdef(CONFIG_BT_MESH_DFU_CLI dfu_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_DFU_SRV dfu_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_DFD_SRV dfd_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_DFU_SLOTS dfu_slot.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_DFU_METADATA dfu_metadata.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_RPR_CLI rpr_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_RPR_SRV rpr_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OD_PRIV_PROXY_CLI od_priv_proxy_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OD_PRIV_PROXY_SRV od_priv_proxy_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SOL_PDU_RPL_CLI sol_pdu_rpl_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_OD_PRIV_PROXY_SRV sol_pdu_rpl_srv.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_BRG_CFG_CLI brg_cfg_cli.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_BRG_CFG_SRV brg_cfg_srv.c brg_cfg.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_SOLICITATION solicitation.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_STATISTIC statistic.c)

embt_library_sources_ifdef(CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG delayable_msg.c)

embt_library_sources_ifdef(CONFIG_BT_TESTING testing.c)
