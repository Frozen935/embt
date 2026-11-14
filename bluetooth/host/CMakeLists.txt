# SPDX-License-Identifier: Apache-2.0

embt_library(embt_host)
embt_library_link_libraries_ifdef(CONFIG_PSA_CRYPTO EmBT::psa)

embt_add_subdirectory_ifdef(CONFIG_BT_CLASSIC classic)
embt_add_subdirectory_ifdef(CONFIG_BT_SHELL shell)

embt_library_sources_ifdef(CONFIG_BT_HCI_RAW          hci_raw.c hci_common.c)
embt_library_sources_ifdef(CONFIG_BT_MONITOR          monitor.c)
embt_library_sources_ifdef(CONFIG_BT_SETTINGS         settings.c)
embt_library_sources_ifdef(CONFIG_BT_HOST_CCM         aes_ccm.c)

if(CONFIG_BT_HCI_HOST)
  embt_library_sources(
    uuid.c
    addr.c
    buf.c
    data.c
    hci_core.c
    hci_common.c
    id.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_BROADCASTER
    adv.c
    )
  embt_library_sources_ifdef(
    CONFIG_BT_OBSERVER
    scan.c
    )

  embt_library_sources_ifdef(
    CONFIG_BT_HOST_CRYPTO
    crypto_psa.c
    )

  embt_library_sources_ifdef(
    CONFIG_BT_ECC
    ecc.c
    )

  if(CONFIG_BT_CONN)
    embt_library_sources(
      conn.c
      l2cap.c
      att.c
      gatt.c
      )

    embt_library_sources_ifdef(
      CONFIG_BT_GATT_GAP_SVC_VALIDATE
      gatt_gap_svc_validate.c
      )

    if(CONFIG_BT_SMP)
      embt_library_sources(
        smp.c
        keys.c
        )
    else()
      embt_library_sources(
        smp_null.c
        )
    endif()
  endif()

  embt_library_sources_ifdef(
    CONFIG_BT_ISO
    iso.c
    conn.c
    )

  embt_library_sources_ifdef(
    CONFIG_BT_CHANNEL_SOUNDING
    cs.c
    )

  if(CONFIG_BT_DF)
    embt_library_sources(
      direction.c
      )
  endif()
endif()

if(CONFIG_BT_OOB_DATA_FIXED)
  message(WARNING "CONFIG_BT_OOB_DATA_FIXED is enabled.
  A hardcoded OOB data set will be stored in the image, do not use in
  production."
    )
endif()
if(CONFIG_BT_USE_DEBUG_KEYS OR CONFIG_BT_STORE_DEBUG_KEYS)
  message(WARNING "One or both these options are enabled:
  CONFIG_BT_USE_DEBUG_KEYS CONFIG_BT_STORE_DEBUG_KEYS.
  A predefined, publicly available keypair intended for testing will be used.
  Do not use in production."
    )
endif()
if(CONFIG_BT_CONN_DISABLE_SECURITY)
  message(WARNING "CONFIG_BT_CONN_DISABLE_SECURITY is enabled.
  Security is disabled for incoming requests for GATT attributes and L2CAP
  channels that would otherwise require encryption/authentication in order to
  be accessed.
  Do not use in production."
    )
endif()
