#include "support/test_support.h"
/*
 * Copyright (c) 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Stub implementations for the external symbols referenced by adv.c that are
 * not exercised by the PAwR response report reassembly tests. Only the HCI
 * command path used by bt_le_ext_adv_create() needs meaningful behaviour; the
 * rest are no-op stubs so that adv.c links in the native unit test build.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/hci_types.h>

#include <embt/net_buf.h>
#include <fff.h>

#include "host/hci_core.h"
#include <host/conn_internal.h>

/* Required by the shared host_mocks assert helper (uses the fff framework). */
DEFINE_FFF_GLOBALS;

/* The single controller state instance referenced by adv.c. */
struct bt_dev bt_dev = {0};

const bt_addr_le_t *bt_lookup_id_addr(uint8_t id, const bt_addr_le_t *addr)
{
	ARG_UNUSED(id);

	return addr;
}

bool bt_addr_le_is_resolved(const bt_addr_le_t *addr)
{
	ARG_UNUSED(addr);

	return false;
}

void bt_addr_le_copy_resolved(bt_addr_le_t *dst, const bt_addr_le_t *src)
{
	*dst = *src;
}

/* Command and response ownership follows the real net_buf pool contract. */
NET_BUF_POOL_FIXED_DEFINE(test_commands, 2, 128, 0, NULL);
NET_BUF_POOL_FIXED_DEFINE(test_responses, 2, sizeof(struct bt_hci_rp_le_set_ext_adv_param), 0, NULL);
static bool pools_initialized;
static void command_pools_init(void)
{
 if (!pools_initialized) {
  net_buf_pool_init(&test_commands);
  net_buf_pool_init(&test_responses);
  pools_initialized = true;
 }
}
struct net_buf *bt_hci_cmd_alloc(bt_timeout_t timeout)
{
 command_pools_init();
 return net_buf_alloc(&test_commands, timeout);
}
int bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf, struct net_buf **rsp)
{
 (void)opcode;
 if (buf) { net_buf_unref(buf); }
 if (rsp) {
  const struct bt_hci_rp_le_set_ext_adv_param response = {0};
  *rsp = net_buf_alloc(&test_responses, BT_NO_WAIT);
  if (!*rsp) { return -ENOMEM; }
  net_buf_add_mem(*rsp, &response, sizeof(response));
 }
 return 0;
}

void bt_hci_cmd_state_set_init(struct net_buf *buf, struct bt_hci_cmd_state_set *state,
			       bt_atomic_t *target, int bit, bool val)
{
	ARG_UNUSED(buf);
	ARG_UNUSED(state);
	ARG_UNUSED(target);
	ARG_UNUSED(bit);
	ARG_UNUSED(val);
}



int bt_get_df_cte_type(uint8_t hci_cte_type)
{
	return hci_cte_type;
}

int bt_id_set_adv_own_addr(struct bt_le_ext_adv *adv, uint32_t options, bool dir_adv,
			   uint8_t *own_addr_type)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(options);
	ARG_UNUSED(dir_adv);

	*own_addr_type = BT_HCI_OWN_ADDR_RANDOM;

	return 0;
}

int bt_id_set_adv_random_addr(struct bt_le_ext_adv *adv, const bt_addr_t *rpa)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(rpa);

	return 0;
}

int bt_id_set_adv_private_addr(struct bt_le_ext_adv *adv, uint32_t options)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(options);

	return 0;
}

void bt_id_save_adv_addr(struct bt_le_ext_adv *adv, uint8_t own_addr_type)
{
	ARG_UNUSED(adv);
	ARG_UNUSED(own_addr_type);
}

int bt_id_set_private_addr(uint8_t id)
{
	ARG_UNUSED(id);

	return 0;
}

bool bt_id_adv_random_addr_check(const struct bt_le_adv_param *param)
{
	ARG_UNUSED(param);

	return true;
}

void bt_id_adv_limited_stopped(struct bt_le_ext_adv *adv)
{
	ARG_UNUSED(adv);
}

void bt_id_pending_keys_update(void)
{
}

int bt_le_scan_set_enable(uint8_t enable)
{
	ARG_UNUSED(enable);

	return 0;
}

void bt_hci_le_enh_conn_complete(struct bt_hci_evt_le_enh_conn_complete *evt,
				 const struct bt_le_ext_adv *ext_adv)
{
	ARG_UNUSED(evt);
	ARG_UNUSED(ext_adv);
}

struct bt_conn *bt_conn_add_le(uint8_t id, const bt_addr_le_t *peer)
{
	ARG_UNUSED(id);
	ARG_UNUSED(peer);

	return NULL;
}

bool bt_conn_exists_le(uint8_t id, const bt_addr_le_t *peer)
{
	ARG_UNUSED(id);
	ARG_UNUSED(peer);

	return false;
}

struct bt_conn *bt_conn_lookup_handle(uint16_t handle, enum bt_conn_type type)
{
	ARG_UNUSED(handle);
	ARG_UNUSED(type);

	return NULL;
}

struct bt_conn *bt_conn_lookup_state_le(uint8_t id, const bt_addr_le_t *peer,
					const bt_conn_state_t state)
{
	ARG_UNUSED(id);
	ARG_UNUSED(peer);
	ARG_UNUSED(state);

	return NULL;
}

void bt_conn_set_state(struct bt_conn *conn, bt_conn_state_t state)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(state);
}

void bt_conn_unref(struct bt_conn *conn)
{
	ARG_UNUSED(conn);
}












int bt_host_work_reschedule(struct bt_work_delayable *work, bt_timeout_t delay)
{
 return bt_work_reschedule(work, delay);
}
enum bt_le_addr_res_support bt_le_bond_addr_res_support(uint8_t id, const bt_addr_le_t *peer)
{
 (void)id; (void)peer;
 fail_msg("Unexpected bonded-peer query in PAwR report reassembly test");
 return BT_LE_ADDR_RES_SUPPORT_UNKNOWN;
}
