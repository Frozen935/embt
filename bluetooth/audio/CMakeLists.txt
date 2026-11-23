# SPDX-License-Identifier: Apache-2.0

embt_add_subdirectory_ifdef(CONFIG_BT_SHELL shell)

embt_library(embt_audio)

embt_library_sources(audio.c)

if(CONFIG_BT_VOCS OR CONFIG_BT_VOCS_CLIENT)
  embt_library_sources(vocs.c)
endif()
embt_library_sources_ifdef(CONFIG_BT_VOCS_CLIENT vocs_client.c)

if(CONFIG_BT_AICS OR CONFIG_BT_AICS_CLIENT)
  embt_library_sources(aics.c)
endif()
embt_library_sources_ifdef(CONFIG_BT_AICS_CLIENT aics_client.c)

embt_library_sources_ifdef(CONFIG_BT_VCP_VOL_REND vcp_vol_rend.c)
embt_library_sources_ifdef(CONFIG_BT_VCP_VOL_CTLR vcp_vol_ctlr.c)

if(CONFIG_BT_MICP_MIC_DEV)
  embt_library_sources(micp_mic_dev.c)
endif()
embt_library_sources_ifdef(CONFIG_BT_MICP_MIC_CTLR micp_mic_ctlr.c)

# When CONFIG_BT_AUDIO and CONFIG_BT_CONN is enabled we pull in ccid. It is based on CONFIG_BT_CONN
# as CCID depends on GATT, and GATT is enabled when CONFIG_BT_CONN=y.
embt_library_sources_ifdef(CONFIG_BT_CONN ccid.c)

embt_library_sources_ifdef(CONFIG_BT_CSIP_SET_MEMBER csip_set_member.c)
embt_library_sources_ifdef(CONFIG_BT_CSIP_SET_COORDINATOR csip_set_coordinator.c)
if(CONFIG_BT_CSIP_SET_MEMBER OR CONFIG_BT_CSIP_SET_COORDINATOR)
  embt_library_sources(csip_crypto.c)
endif()

embt_library_sources_ifdef(CONFIG_BT_TBS tbs.c)

embt_library_sources_ifdef(CONFIG_BT_TBS_CLIENT tbs_client.c)

embt_library_sources_ifdef(CONFIG_BT_MCC mcc.c)

embt_library_sources_ifdef(CONFIG_BT_MCS mcs.c)

embt_library_sources_ifdef(CONFIG_BT_MCP_MEDIA_CONTROL_SERVER mcp_media_control_server.c)

embt_library_sources_ifdef(CONFIG_BT_ASCS ascs.c)
embt_library_sources_ifdef(CONFIG_BT_PACS pacs.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_STREAM bap_stream.c codec.c bap_iso.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_BASE bap_base.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_UNICAST_SERVER bap_unicast_server.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_UNICAST_CLIENT bap_unicast_client.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_BROADCAST_SOURCE bap_broadcast_source.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_BROADCAST_SINK bap_broadcast_sink.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_SCAN_DELEGATOR bap_scan_delegator.c)
embt_library_sources_ifdef(CONFIG_BT_BAP_BROADCAST_ASSISTANT bap_broadcast_assistant.c)
embt_library_sources_ifdef(CONFIG_BT_CCP_CALL_CONTROL_CLIENT ccp_call_control_client.c)
embt_library_sources_ifdef(CONFIG_BT_CCP_CALL_CONTROL_SERVER ccp_call_control_server.c)
embt_library_sources_ifdef(CONFIG_BT_HAS has.c)
embt_library_sources_ifdef(CONFIG_BT_HAS_CLIENT has_client.c)
embt_library_sources_ifdef(CONFIG_BT_CAP cap_stream.c)
embt_library_sources_ifdef(CONFIG_BT_CAP_ACCEPTOR cap_acceptor.c)
embt_library_sources_ifdef(CONFIG_BT_CAP_INITIATOR cap_initiator.c)
embt_library_sources_ifdef(CONFIG_BT_CAP_COMMANDER cap_commander.c)
embt_library_sources_ifdef(CONFIG_BT_CAP_HANDOVER cap_handover.c)
if(CONFIG_BT_CAP_INITIATOR_UNICAST OR CONFIG_BT_CAP_COMMANDER)
  embt_library_sources(cap_common.c)
endif()
embt_library_sources_ifdef(CONFIG_BT_TMAP tmap.c)
embt_library_sources_ifdef(CONFIG_BT_GMAP gmap_client.c gmap_server.c)
embt_library_sources_ifdef(CONFIG_BT_PBP pbp.c)
