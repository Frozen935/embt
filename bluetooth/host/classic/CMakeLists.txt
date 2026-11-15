# SPDX-License-Identifier: Apache-2.0

embt_add_subdirectory_ifdef(CONFIG_BT_SHELL shell)

embt_library(embt_classic)
embt_library_link_libraries_ifdef(CONFIG_EMBT_MBEDTLS_HELPERS EmBT::psa)

embt_library_sources_ifdef(CONFIG_BT_A2DP             a2dp.c a2dp_codec_sbc.c)
embt_library_sources_ifdef(CONFIG_BT_AVDTP            avdtp.c)
embt_library_sources_ifdef(CONFIG_BT_AVRCP            avrcp.c)
embt_library_sources_ifdef(CONFIG_BT_AVCTP            avctp.c)
embt_library_sources_ifdef(CONFIG_BT_RFCOMM           rfcomm.c)
embt_library_sources_ifdef(CONFIG_BT_BIP              bip.c)
embt_library_sources_ifdef(CONFIG_BT_MAP              map.c)
embt_library_sources_ifdef(CONFIG_BT_FTP              ftp.c)
embt_library_sources_ifdef(CONFIG_BT_OPP              opp.c)

embt_library_sources_ifdef(
  CONFIG_BT_CLASSIC
  br.c
  conn_br.c
  keys_br.c
  l2cap_br.c
  sdp.c
  ssp.c
  sco.c
  )

embt_library_sources_ifdef(
  CONFIG_BT_HFP_HF
  hfp_hf.c
  at.c
  )

embt_library_sources_ifdef(
  CONFIG_BT_HFP_AG
  hfp_ag.c
  )

embt_library_sources_ifdef(
  CONFIG_BT_GOEP
  obex.c
  goep.c
  )

embt_library_sources_ifdef(
  CONFIG_BT_DID
  did.c
  )

embt_library_sources_ifdef(
  CONFIG_BT_HID_DEVICE
  hid_device.c
  )

embt_library_sources_ifdef(
  CONFIG_BT_HID_HOST
  hid_host.c
  )

if(CONFIG_BT_AVRCP_CT_COVER_ART OR CONFIG_BT_AVRCP_TG_COVER_ART)
  embt_library_sources(avrcp_cover_art.c)
endif()

embt_library_sources_ifdef(
  CONFIG_BT_PBAP
  pbap.c
  )

if(CONFIG_BT_OBEX_AUTH)
  message(WARNING "CONFIG_BT_OBEX_AUTH: OBEX uses MD5 for authentication, which is "
                  "cryptographically weak.")
endif()
