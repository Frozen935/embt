/**
 * @file bt_shell_private.c
 * @brief Bluetooth shell private module
 *
 * Provide common function which can be shared using privately inside Bluetooth shell.
 */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bt_shell_private.h"

static void wall_vfprintf(enum bt_shell_color color, const char *fmt, va_list args)
{
	bt_shell_vfprintf(NULL, color, fmt, args);
}

void bt_shell_hexdump(const uint8_t *data, size_t len)
{
	bt_shell_hexdump_to(NULL, data, len);
}

void bt_shell_fprintf(enum bt_shell_color color,
		      const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(color, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_INFO, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_NORMAL, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_WARNING, fmt, args);
	va_end(args);
}

void bt_shell_fprintf_error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	wall_vfprintf(BT_SHELL_ERROR, fmt, args);
	va_end(args);
}

extern const struct bt_shell_cmd_entry bt_shell_root_bt;
extern const struct bt_shell_cmd_entry bt_shell_root_gatt;
extern const struct bt_shell_cmd_entry bt_shell_root_l2cap;
extern const struct bt_shell_cmd_entry bt_shell_root_br;
void bt_shell_br_pools_init(void);
int bt_shell_bt_callbacks_init(void);
int bt_shell_l2cap_callbacks_init(void);

#if (defined(CONFIG_BT_HAS_PRESET_SUPPORT))
extern const struct bt_shell_cmd_entry bt_shell_root_has;
#endif
#if (defined(CONFIG_BT_CCP_CALL_CONTROL_CLIENT))
extern const struct bt_shell_cmd_entry bt_shell_root_ccp_call_control_client;
#endif
#if (defined(CONFIG_BT_HAS_CLIENT))
extern const struct bt_shell_cmd_entry bt_shell_root_has_client;
#endif
#if (defined(CONFIG_BT_CCP_CALL_CONTROL_SERVER))
extern const struct bt_shell_cmd_entry bt_shell_root_ccp_call_control_server;
#endif
#if (defined(CONFIG_BT_VCP_VOL_REND))
extern const struct bt_shell_cmd_entry bt_shell_root_vcp_vol_rend;
#endif
#if (defined(CONFIG_BT_TMAP))
extern const struct bt_shell_cmd_entry bt_shell_root_tmap;
#endif
#if (defined(CONFIG_BT_TBS))
extern const struct bt_shell_cmd_entry bt_shell_root_tbs;
#endif
#if (defined(CONFIG_BT_PBP))
extern const struct bt_shell_cmd_entry bt_shell_root_pbp;
#endif
#if (defined(CONFIG_BT_CSIP_SET_MEMBER))
extern const struct bt_shell_cmd_entry bt_shell_root_csip_set_member;
#endif
#if (defined(CONFIG_BT_TBS_CLIENT))
extern const struct bt_shell_cmd_entry bt_shell_root_tbs_client;
#endif
#if (defined(CONFIG_BT_BAP_SCAN_DELEGATOR))
extern const struct bt_shell_cmd_entry bt_shell_root_bap_scan_delegator;
#endif
#if (defined(CONFIG_BT_MICP_MIC_DEV))
extern const struct bt_shell_cmd_entry bt_shell_root_micp_mic_dev;
#endif
#if (defined(CONFIG_BT_VCP_VOL_CTLR))
extern const struct bt_shell_cmd_entry bt_shell_root_vcp_vol_ctlr;
#endif
#if (defined(CONFIG_BT_MICP_MIC_CTLR))
extern const struct bt_shell_cmd_entry bt_shell_root_micp_mic_ctlr;
#endif
#if (defined(CONFIG_BT_CAP_HANDOVER))
extern const struct bt_shell_cmd_entry bt_shell_root_cap_handover;
#endif
#if (defined(CONFIG_BT_MCC))
extern const struct bt_shell_cmd_entry bt_shell_root_mcc;
#endif
#if (defined(CONFIG_BT_CSIP_SET_COORDINATOR))
extern const struct bt_shell_cmd_entry bt_shell_root_csip_set_coordinator;
#endif
#if (defined(CONFIG_BT_CAP_INITIATOR))
extern const struct bt_shell_cmd_entry bt_shell_root_cap_initiator;
#endif
#if (defined(CONFIG_BT_CAP_ACCEPTOR)) && ((defined(CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER)))
extern const struct bt_shell_cmd_entry bt_shell_root_cap_acceptor;
#endif
#if (defined(CONFIG_BT_BAP_BROADCAST_ASSISTANT))
extern const struct bt_shell_cmd_entry bt_shell_root_bap_broadcast_assistant;
#endif
#if (defined(CONFIG_BT_GMAP))
extern const struct bt_shell_cmd_entry bt_shell_root_gmap;
#endif
#if (defined(CONFIG_BT_CAP_COMMANDER))
extern const struct bt_shell_cmd_entry bt_shell_root_cap_commander;
#endif
#if (defined(CONFIG_BT_BAP_STREAM))
extern const struct bt_shell_cmd_entry bt_shell_root_bap;
#endif
#if (defined(CONFIG_BT_ISO))
extern const struct bt_shell_cmd_entry bt_shell_root_iso;
#endif
#if (defined(CONFIG_BT_CHANNEL_SOUNDING))
extern const struct bt_shell_cmd_entry bt_shell_root_cs;
#endif
#if (defined(CONFIG_BT_HFP_HF) || defined(CONFIG_BT_HFP_AG))
extern const struct bt_shell_cmd_entry bt_shell_root_hfp;
#endif
#if (defined(CONFIG_BT_HID_DEVICE))
extern const struct bt_shell_cmd_entry bt_shell_root_hid_device;
#endif
#if (defined(CONFIG_BT_RFCOMM))
extern const struct bt_shell_cmd_entry bt_shell_root_spp;
#endif
#if (defined(CONFIG_BT_AVRCP_CT_COVER_ART) || defined(CONFIG_BT_AVRCP_TG_COVER_ART))
extern const struct bt_shell_cmd_entry bt_shell_root_avrcp_cover_art;
#endif
#if (defined(CONFIG_BT_BIP))
extern const struct bt_shell_cmd_entry bt_shell_root_bip;
#endif
#if (defined(CONFIG_BT_MAP))
extern const struct bt_shell_cmd_entry bt_shell_root_map;
#endif
#if (defined(CONFIG_BT_A2DP))
extern const struct bt_shell_cmd_entry bt_shell_root_a2dp;
#endif
#if (defined(CONFIG_BT_OPP))
extern const struct bt_shell_cmd_entry bt_shell_root_opp;
#endif
#if (defined(CONFIG_BT_FTP))
extern const struct bt_shell_cmd_entry bt_shell_root_ftp;
#endif
#if (defined(CONFIG_BT_HID_HOST))
extern const struct bt_shell_cmd_entry bt_shell_root_hid_host;
#endif
#if (defined(CONFIG_BT_GOEP))
extern const struct bt_shell_cmd_entry bt_shell_root_goep;
#endif
#if (defined(CONFIG_BT_PBAP))
extern const struct bt_shell_cmd_entry bt_shell_root_pbap;
#endif
#if (defined(CONFIG_BT_AVRCP))
extern const struct bt_shell_cmd_entry bt_shell_root_avrcp;
#endif
#if (defined(CONFIG_BT_IAS_CLIENT))
extern const struct bt_shell_cmd_entry bt_shell_root_ias_client;
#endif
#if (defined(CONFIG_BT_IAS))
extern const struct bt_shell_cmd_entry bt_shell_root_ias;
#endif
#if (defined(CONFIG_BT_MESH_SHELL))
extern const struct bt_shell_cmd_entry bt_shell_root_mesh;
#endif

int bt_shell_commands_init(struct bt_shell *sh)
{
	int err;

	err = bt_shell_register(sh, &bt_shell_root_bt);
	if (err) {
		return err;
	}
#if defined(CONFIG_BT_CONN)
	err = bt_shell_register(sh, &bt_shell_root_gatt);
	if (err) {
		return err;
	}
#endif
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	err = bt_shell_register(sh, &bt_shell_root_l2cap);
	if (err) {
		return err;
	}
#endif
#if defined(CONFIG_BT_CLASSIC)
	bt_shell_br_pools_init();
	err = bt_shell_register(sh, &bt_shell_root_br);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_HAS_PRESET_SUPPORT))
	err = bt_shell_register(sh, &bt_shell_root_has);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CCP_CALL_CONTROL_CLIENT))
	err = bt_shell_register(sh, &bt_shell_root_ccp_call_control_client);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_HAS_CLIENT))
	err = bt_shell_register(sh, &bt_shell_root_has_client);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CCP_CALL_CONTROL_SERVER))
	err = bt_shell_register(sh, &bt_shell_root_ccp_call_control_server);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_VCP_VOL_REND))
	err = bt_shell_register(sh, &bt_shell_root_vcp_vol_rend);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_TMAP))
	err = bt_shell_register(sh, &bt_shell_root_tmap);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_TBS))
	err = bt_shell_register(sh, &bt_shell_root_tbs);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_PBP))
	err = bt_shell_register(sh, &bt_shell_root_pbp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CSIP_SET_MEMBER))
	err = bt_shell_register(sh, &bt_shell_root_csip_set_member);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_TBS_CLIENT))
	err = bt_shell_register(sh, &bt_shell_root_tbs_client);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_BAP_SCAN_DELEGATOR))
	err = bt_shell_register(sh, &bt_shell_root_bap_scan_delegator);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_MICP_MIC_DEV))
	err = bt_shell_register(sh, &bt_shell_root_micp_mic_dev);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_VCP_VOL_CTLR))
	err = bt_shell_register(sh, &bt_shell_root_vcp_vol_ctlr);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_MICP_MIC_CTLR))
	err = bt_shell_register(sh, &bt_shell_root_micp_mic_ctlr);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CAP_HANDOVER))
	err = bt_shell_register(sh, &bt_shell_root_cap_handover);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_MCC))
	err = bt_shell_register(sh, &bt_shell_root_mcc);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CSIP_SET_COORDINATOR))
	err = bt_shell_register(sh, &bt_shell_root_csip_set_coordinator);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CAP_INITIATOR))
	err = bt_shell_register(sh, &bt_shell_root_cap_initiator);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CAP_ACCEPTOR)) && ((defined(CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER)))
	err = bt_shell_register(sh, &bt_shell_root_cap_acceptor);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_BAP_BROADCAST_ASSISTANT))
	err = bt_shell_register(sh, &bt_shell_root_bap_broadcast_assistant);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_GMAP))
	err = bt_shell_register(sh, &bt_shell_root_gmap);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CAP_COMMANDER))
	err = bt_shell_register(sh, &bt_shell_root_cap_commander);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_BAP_STREAM))
	err = bt_shell_register(sh, &bt_shell_root_bap);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_ISO))
	err = bt_shell_register(sh, &bt_shell_root_iso);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_CHANNEL_SOUNDING))
	err = bt_shell_register(sh, &bt_shell_root_cs);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_HFP_HF) || defined(CONFIG_BT_HFP_AG))
	err = bt_shell_register(sh, &bt_shell_root_hfp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_HID_DEVICE))
	err = bt_shell_register(sh, &bt_shell_root_hid_device);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_RFCOMM))
	err = bt_shell_register(sh, &bt_shell_root_spp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_AVRCP_CT_COVER_ART) || defined(CONFIG_BT_AVRCP_TG_COVER_ART))
	err = bt_shell_register(sh, &bt_shell_root_avrcp_cover_art);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_BIP))
	err = bt_shell_register(sh, &bt_shell_root_bip);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_MAP))
	err = bt_shell_register(sh, &bt_shell_root_map);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_A2DP))
	err = bt_shell_register(sh, &bt_shell_root_a2dp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_OPP))
	err = bt_shell_register(sh, &bt_shell_root_opp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_FTP))
	err = bt_shell_register(sh, &bt_shell_root_ftp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_HID_HOST))
	err = bt_shell_register(sh, &bt_shell_root_hid_host);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_GOEP))
	err = bt_shell_register(sh, &bt_shell_root_goep);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_PBAP))
	err = bt_shell_register(sh, &bt_shell_root_pbap);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_AVRCP))
	err = bt_shell_register(sh, &bt_shell_root_avrcp);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_IAS_CLIENT))
	err = bt_shell_register(sh, &bt_shell_root_ias_client);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_IAS))
	err = bt_shell_register(sh, &bt_shell_root_ias);
	if (err) {
		return err;
	}
#endif
#if (defined(CONFIG_BT_MESH_SHELL))
	err = bt_shell_register(sh, &bt_shell_root_mesh);
	if (err) {
		return err;
	}
#endif
	err = bt_shell_bt_callbacks_init();
	if (err) {
		return err;
	}
#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	return bt_shell_l2cap_callbacks_init();
#else
	return 0;
#endif
}

int bt_shell_storage_init(void)
{
#if defined(CONFIG_BT_BAP_STREAM)
	{
		extern void bt_audio_shell_bap_storage_init(void);

		bt_audio_shell_bap_storage_init();
	}
#endif
#if defined(CONFIG_BT_BAP_BROADCAST_ASSISTANT)
	{
		extern void bt_audio_shell_bap_broadcast_assistant_storage_init(void);

		bt_audio_shell_bap_broadcast_assistant_storage_init();
	}
#endif
#if defined(CONFIG_BT_BAP_SCAN_DELEGATOR)
	{
		extern void bt_audio_shell_bap_scan_delegator_storage_init(void);

		bt_audio_shell_bap_scan_delegator_storage_init();
	}
#endif
#if defined(CONFIG_BT_CSIP_SET_COORDINATOR)
	{
		extern void bt_audio_shell_csip_set_coordinator_storage_init(void);

		bt_audio_shell_csip_set_coordinator_storage_init();
	}
#endif
#if defined(CONFIG_BT_ISO)
	{
		extern void bt_host_shell_iso_storage_init(void);

		bt_host_shell_iso_storage_init();
	}
#endif
#if defined(CONFIG_BT_HID_DEVICE)
	{
		extern void bt_host_classic_shell_hid_device_storage_init(void);

		bt_host_classic_shell_hid_device_storage_init();
	}
#endif
#if defined(CONFIG_BT_RFCOMM)
	{
		extern void bt_host_classic_shell_spp_storage_init(void);

		bt_host_classic_shell_spp_storage_init();
	}
#endif
#if defined(CONFIG_BT_AVRCP_CT_COVER_ART) || defined(CONFIG_BT_AVRCP_TG_COVER_ART)
	{
		extern void bt_host_classic_shell_avrcp_cover_art_storage_init(void);

		bt_host_classic_shell_avrcp_cover_art_storage_init();
	}
#endif
#if defined(CONFIG_BT_BIP)
	{
		extern void bt_host_classic_shell_bip_storage_init(void);

		bt_host_classic_shell_bip_storage_init();
	}
#endif
#if defined(CONFIG_BT_MAP)
	{
		extern void bt_host_classic_shell_map_storage_init(void);

		bt_host_classic_shell_map_storage_init();
	}
#endif
#if defined(CONFIG_BT_A2DP)
	{
		extern void bt_host_classic_shell_a2dp_storage_init(void);

		bt_host_classic_shell_a2dp_storage_init();
	}
#endif
#if defined(CONFIG_BT_OPP)
	{
		extern void bt_host_classic_shell_opp_storage_init(void);

		bt_host_classic_shell_opp_storage_init();
	}
#endif
#if defined(CONFIG_BT_FTP)
	{
		extern void bt_host_classic_shell_ftp_storage_init(void);

		bt_host_classic_shell_ftp_storage_init();
	}
#endif
#if defined(CONFIG_BT_HID_HOST)
	{
		extern void bt_host_classic_shell_hid_host_storage_init(void);

		bt_host_classic_shell_hid_host_storage_init();
	}
#endif
#if defined(CONFIG_BT_GOEP)
	{
		extern void bt_host_classic_shell_goep_storage_init(void);

		bt_host_classic_shell_goep_storage_init();
	}
#endif
#if defined(CONFIG_BT_PBAP)
	{
		extern void bt_host_classic_shell_pbap_storage_init(void);

		bt_host_classic_shell_pbap_storage_init();
	}
#endif
#if defined(CONFIG_BT_AVRCP)
	{
		extern void bt_host_classic_shell_avrcp_storage_init(void);

		bt_host_classic_shell_avrcp_storage_init();
	}
#endif
	return 0;
}
