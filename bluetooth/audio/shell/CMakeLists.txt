# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_CCP_CALL_CONTROL_SERVER OR
   CONFIG_BT_CCP_CALL_CONTROL_CLIENT OR
   CONFIG_BT_VCP_VOL_REND OR
   CONFIG_BT_VCP_VOL_CTLR OR
   CONFIG_BT_MICP_MIC_DEV OR
   CONFIG_BT_MICP_MIC_CTLR OR
   CONFIG_BT_CSIP_SET_MEMBER OR
   CONFIG_BT_CSIP_SET_COORDINATOR OR
   CONFIG_BT_TBS OR
   CONFIG_BT_TBS_CLIENT OR
   CONFIG_BT_MCC OR
   CONFIG_BT_HAS_PRESET_SUPPORT OR
   CONFIG_BT_CAP_ACCEPTOR OR
   CONFIG_BT_CAP_INITIATOR OR
   CONFIG_BT_CAP_COMMANDER OR
   CONFIG_BT_CAP_HANDOVER OR
   CONFIG_BT_HAS_CLIENT OR
   CONFIG_BT_TMAP OR
   CONFIG_BT_GMAP OR
   CONFIG_BT_BAP_STREAM OR
   CONFIG_BT_BAP_SCAN_DELEGATOR OR
   CONFIG_BT_BAP_BROADCAST_ASSISTANT OR
   CONFIG_BT_PBP)
  embt_library(embt_audio_shell)

  embt_library_sources_ifdef(
    CONFIG_BT_CCP_CALL_CONTROL_SERVER
    ccp_call_control_server.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CCP_CALL_CONTROL_CLIENT
    ccp_call_control_client.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_VCP_VOL_REND
    vcp_vol_rend.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_VCP_VOL_CTLR
    vcp_vol_ctlr.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_MICP_MIC_DEV
    micp_mic_dev.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_MICP_MIC_CTLR
    micp_mic_ctlr.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CSIP_SET_MEMBER
    csip_set_member.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CSIP_SET_COORDINATOR
    csip_set_coordinator.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_TBS
    tbs.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_TBS_CLIENT
    tbs_client.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_MCC
    mcc.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_HAS_PRESET_SUPPORT
    has.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CAP_ACCEPTOR
    cap_acceptor.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CAP_INITIATOR
    cap_initiator.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CAP_COMMANDER
    cap_commander.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_CAP_HANDOVER
    cap_handover.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_HAS_CLIENT
    has_client.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_TMAP
    tmap.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_GMAP
    gmap.c
    )
  # We use BT_BAP_STREAM as a common ground for audio, as that is set whenever
  # any audio stream functionality is enabled.
  embt_library_sources_ifdef(
    CONFIG_BT_BAP_STREAM
    bap.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_BAP_SCAN_DELEGATOR
    bap_scan_delegator.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_BAP_BROADCAST_ASSISTANT
    bap_broadcast_assistant.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_PBP
    pbp.c
    )
endif()
