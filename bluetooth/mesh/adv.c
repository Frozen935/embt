/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <embt/base/fifo.h>
#include <embt/base/base.h>
#include <embt/base/time.h>
#include <embt/osi/sync.h>
#include <embt/base/mem_slab.h>
#include <embt/base/time.h>
#include <embt/base/atomic.h>
#include <string.h>
#include <embt/base/assert.h>
#include <embt/base/util.h>

#include <embt/net_buf.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/hci.h>
#include <embt/bluetooth/conn.h>
#include <embt/bluetooth/mesh.h>

#include "common/bt_str.h"

#include "net.h"
#include "foundation.h"
#include "beacon.h"
#include "prov.h"
#include "proxy.h"
#include "pb_gatt_srv.h"
#include "solicitation.h"
#include "statistic.h"
#include <embt/base/log.h>

LOG_MODULE_REGISTER(bt_mesh_adv, CONFIG_BT_MESH_ADV_LOG_LEVEL);

/* Window and Interval are equal for continuous scanning */
#define MESH_SCAN_INTERVAL    BT_MESH_ADV_SCAN_UNIT(BT_MESH_SCAN_INTERVAL_MS)
#define MESH_SCAN_WINDOW      BT_MESH_ADV_SCAN_UNIT(BT_MESH_SCAN_WINDOW_MS)

const uint8_t bt_mesh_adv_type[BT_MESH_ADV_TYPES] = {
	[BT_MESH_ADV_PROV]   = BT_DATA_MESH_PROV,
	[BT_MESH_ADV_DATA]   = BT_DATA_MESH_MESSAGE,
	[BT_MESH_ADV_BEACON] = BT_DATA_MESH_BEACON,
	[BT_MESH_ADV_URI]    = BT_DATA_URI,
};

static bool active_scanning;
static os_sem_t adv_ready;
static bt_atomic_t adv_cancel_generation;
static BT_FIFO_DEFINE(bt_mesh_adv_queue);
static BT_FIFO_DEFINE(bt_mesh_relay_queue);
static BT_FIFO_DEFINE(bt_mesh_friend_queue);

BT_MEM_SLAB_DEFINE_STATIC_TYPE(local_adv_pool, struct bt_mesh_adv,
			      CONFIG_BT_MESH_ADV_BUF_COUNT);

#if defined(CONFIG_BT_MESH_RELAY_BUF_COUNT)
BT_MEM_SLAB_DEFINE_STATIC_TYPE(relay_adv_pool, struct bt_mesh_adv,
			      CONFIG_BT_MESH_RELAY_BUF_COUNT);
#endif

#if defined(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
BT_MEM_SLAB_DEFINE_STATIC_TYPE(friend_adv_pool, struct bt_mesh_adv,
			      CONFIG_BT_MESH_FRIEND_LPN_COUNT);
#endif

void bt_mesh_adv_send_start(uint16_t duration, int err, struct bt_mesh_adv_ctx *ctx)
{
	if (!ctx->started) {
		ctx->started = 1;

		if (ctx->cb && ctx->cb->start) {
			ctx->cb->start(duration, err, ctx->cb_data);
		}

		if (err) {
			ctx->cb = NULL;
		} else if (IS_ENABLED(CONFIG_BT_MESH_STATISTIC)) {
			bt_mesh_stat_succeeded_count(ctx);
		}
	}
}

void bt_mesh_adv_send_end(int err, struct bt_mesh_adv_ctx const *ctx)
{
	if (ctx->started && ctx->cb && ctx->cb->end) {
		ctx->cb->end(err, ctx->cb_data);
	}
}

static struct bt_mesh_adv *adv_create_from_pool(struct bt_mem_slab *buf_pool,
						enum bt_mesh_adv_type type,
						enum bt_mesh_adv_tag tag,
						uint8_t xmit, bt_timeout_t timeout)
{
	struct bt_mesh_adv_ctx *ctx;
	struct bt_mesh_adv *adv;
	int err;

	if (bt_atomic_test_bit(bt_mesh.flags, BT_MESH_SUSPENDED)) {
		LOG_WRN("Refusing to allocate buffer while suspended");
		return NULL;
	}

	err = bt_mem_slab_alloc(buf_pool, (void **)&adv, timeout);
	if (err) {
		return NULL;
	}

	adv->__ref = 1;

	net_buf_simple_init_with_data(&adv->b, adv->__bufs, BT_MESH_ADV_DATA_SIZE);
	net_buf_simple_reset(&adv->b);

	ctx = &adv->ctx;

	(void)memset(ctx, 0, sizeof(*ctx));

	ctx->type         = type;
	ctx->tag          = tag;
	ctx->xmit         = xmit;

	return adv;
}

struct bt_mesh_adv *bt_mesh_adv_ref(struct bt_mesh_adv *adv)
{
	__ASSERT_NO_MSG(adv->__ref < UINT8_MAX);

	adv->__ref++;

	return adv;
}

void bt_mesh_adv_unref(struct bt_mesh_adv *adv)
{
	__ASSERT_NO_MSG(adv->__ref > 0);

	if (--adv->__ref > 0) {
		return;
	}

	struct bt_mem_slab *slab = &local_adv_pool;

#if (defined(CONFIG_BT_MESH_RELAY) || defined(CONFIG_BT_MESH_BRG_CFG_SRV))
	if (adv->ctx.tag == BT_MESH_ADV_TAG_RELAY) {
		slab = &relay_adv_pool;
	}
#endif

#if defined(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
	if (adv->ctx.tag == BT_MESH_ADV_TAG_FRIEND) {
		slab = &friend_adv_pool;
	}
#endif

	bt_mem_slab_free(slab, (void *)adv);
}

struct bt_mesh_adv *bt_mesh_adv_create(enum bt_mesh_adv_type type,
				       enum bt_mesh_adv_tag tag,
				       uint8_t xmit, bt_timeout_t timeout)
{
#if (defined(CONFIG_BT_MESH_RELAY) || defined(CONFIG_BT_MESH_BRG_CFG_SRV))
	if (tag == BT_MESH_ADV_TAG_RELAY) {
		return adv_create_from_pool(&relay_adv_pool,
					    type, tag, xmit, timeout);
	}
#endif

#if defined(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
	if (tag == BT_MESH_ADV_TAG_FRIEND) {
		return adv_create_from_pool(&friend_adv_pool,
					    type, tag, xmit, timeout);
	}
#endif

	return adv_create_from_pool(&local_adv_pool, type,
				    tag, xmit, timeout);
}

struct bt_mesh_adv *bt_mesh_adv_get(bt_timeout_t timeout)
{
	bt_timepoint_t end = bt_timepoint_calc(timeout);
	bt_atomic_val_t generation = bt_atomic_get(&adv_cancel_generation);
	struct bt_mesh_adv *adv;

	for (;;) {
		adv = bt_fifo_get(&bt_mesh_adv_queue, BT_NO_WAIT);
#if (defined(CONFIG_BT_MESH_RELAY) || defined(CONFIG_BT_MESH_BRG_CFG_SRV)) && \
	(defined(CONFIG_BT_MESH_ADV_LEGACY) || \
	 defined(CONFIG_BT_MESH_ADV_EXT_RELAY_USING_MAIN_ADV_SET) || \
	 !(CONFIG_BT_MESH_RELAY_ADV_SETS))
		if (!adv) {
			adv = bt_fifo_get(&bt_mesh_relay_queue, BT_NO_WAIT);
		}
#endif
		if (adv || bt_timepoint_expired(end)) {
			return adv;
		}
		if (osi_sem_take(&adv_ready, bt_timepoint_timeout(end)) ||
		    generation != bt_atomic_get(&adv_cancel_generation)) {
			return NULL;
		}
	}
}

struct bt_mesh_adv *bt_mesh_adv_get_by_tag(enum bt_mesh_adv_tag_bit tags, bt_timeout_t timeout)
{
	if (IS_ENABLED(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE) &&
	    tags & BT_MESH_ADV_TAG_BIT_FRIEND) {
		return bt_fifo_get(&bt_mesh_friend_queue, timeout);
	}

	if ((IS_ENABLED(CONFIG_BT_MESH_RELAY) || IS_ENABLED(CONFIG_BT_MESH_BRG_CFG_SRV)) &&
	    !(tags & BT_MESH_ADV_TAG_BIT_LOCAL)) {
		return bt_fifo_get(&bt_mesh_relay_queue, timeout);
	}

	if (IS_ENABLED(CONFIG_BT_MESH_ADV_EXT_GATT_SEPARATE) &&
	    tags & BT_MESH_ADV_TAG_BIT_PROXY) {
		return NULL;
	}

	return bt_mesh_adv_get(timeout);
}

void bt_mesh_adv_get_cancel(void)
{
	LOG_DBG("");

	bt_atomic_inc(&adv_cancel_generation);
	bt_base_require(osi_sem_give(&adv_ready));

	bt_fifo_cancel_wait(&bt_mesh_adv_queue);

	if ((IS_ENABLED(CONFIG_BT_MESH_RELAY) || IS_ENABLED(CONFIG_BT_MESH_BRG_CFG_SRV))) {
		bt_fifo_cancel_wait(&bt_mesh_relay_queue);
	}

	if (IS_ENABLED(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)) {
		bt_fifo_cancel_wait(&bt_mesh_friend_queue);
	}
}

void bt_mesh_adv_send(struct bt_mesh_adv *adv, const struct bt_mesh_send_cb *cb,
		      void *cb_data)
{
	LOG_DBG("type 0x%02x len %u: %s", adv->ctx.type, adv->b.len,
		bt_hex(adv->b.data, adv->b.len));

	if (bt_atomic_test_bit(bt_mesh.flags, BT_MESH_SUSPENDED)) {
		LOG_WRN("Sending advertisement while suspended");
	}

	adv->ctx.cb = cb;
	adv->ctx.cb_data = cb_data;
	adv->ctx.busy = 1U;

	if (IS_ENABLED(CONFIG_BT_MESH_STATISTIC)) {
		bt_mesh_stat_planned_count(&adv->ctx);
	}

	if (IS_ENABLED(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE) &&
	    adv->ctx.tag == BT_MESH_ADV_TAG_FRIEND) {
		bt_fifo_put(&bt_mesh_friend_queue, bt_mesh_adv_ref(adv));
		bt_mesh_adv_friend_ready();
		return;
	}

	if (((IS_ENABLED(CONFIG_BT_MESH_RELAY) || IS_ENABLED(CONFIG_BT_MESH_BRG_CFG_SRV)) &&
	    adv->ctx.tag == BT_MESH_ADV_TAG_RELAY) ||
	    (IS_ENABLED(CONFIG_BT_MESH_PB_ADV_USE_RELAY_SETS) &&
	     adv->ctx.tag == BT_MESH_ADV_TAG_PROV)) {
		bt_fifo_put(&bt_mesh_relay_queue, bt_mesh_adv_ref(adv));
		bt_base_require(osi_sem_give(&adv_ready));
		bt_mesh_adv_relay_ready();
		return;
	}

	bt_fifo_put(&bt_mesh_adv_queue, bt_mesh_adv_ref(adv));
	bt_base_require(osi_sem_give(&adv_ready));
	bt_mesh_adv_local_ready();
}

int bt_mesh_adv_gatt_send(void)
{
	if (bt_mesh_is_provisioned()) {
		if (IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY)) {
			LOG_DBG("Proxy Advertising");
			return bt_mesh_proxy_adv_start();
		}
	} else if (IS_ENABLED(CONFIG_BT_MESH_PB_GATT)) {
		LOG_DBG("PB-GATT Advertising");
		return bt_mesh_pb_gatt_srv_adv_start();
	}

	return -ENOTSUP;
}

static void bt_mesh_scan_cb(const bt_addr_le_t *addr, int8_t rssi,
			    uint8_t adv_type, struct net_buf_simple *buf)
{
	if (adv_type != BT_GAP_ADV_TYPE_ADV_NONCONN_IND) {
		return;
	}

	LOG_DBG("len %u: %s", buf->len, bt_hex(buf->data, buf->len));

	while (buf->len > 1) {
		struct net_buf_simple_state state;
		uint8_t len, type;

		len = net_buf_simple_pull_u8(buf);
		/* Check for early termination */
		if (len == 0U) {
			return;
		}

		if (len > buf->len) {
			LOG_WRN("AD malformed");
			return;
		}

		net_buf_simple_save(buf, &state);

		type = net_buf_simple_pull_u8(buf);

		buf->len = len - 1;

		switch (type) {
		case BT_DATA_MESH_MESSAGE:
			bt_mesh_net_recv(buf, rssi, BT_MESH_NET_IF_ADV);
			break;
#if defined(CONFIG_BT_MESH_PB_ADV)
		case BT_DATA_MESH_PROV:
			bt_mesh_pb_adv_recv(buf);
			break;
#endif
		case BT_DATA_MESH_BEACON:
			bt_mesh_beacon_recv(buf);
			break;
		case BT_DATA_UUID16_SOME:
			/* Fall through */
		case BT_DATA_UUID16_ALL:
			if (IS_ENABLED(CONFIG_BT_MESH_OD_PRIV_PROXY_SRV)) {
				/* Restore buffer with Solicitation PDU */
				net_buf_simple_restore(buf, &state);
				bt_mesh_sol_recv(buf, len - 1);
			}
			break;
		default:
			break;
		}

		net_buf_simple_restore(buf, &state);
		net_buf_simple_pull(buf, len);
	}
}

int bt_mesh_scan_active_set(bool active)
{
	if (active_scanning == active) {
		return 0;
	}

	active_scanning = active;
	bt_mesh_scan_disable();
	return bt_mesh_scan_enable();
}

int bt_mesh_scan_enable(void)
{
	struct bt_le_scan_param scan_param = {
		.type = active_scanning ? BT_LE_SCAN_TYPE_ACTIVE :
					  BT_LE_SCAN_TYPE_PASSIVE,
		.interval = MESH_SCAN_INTERVAL,
		.window = MESH_SCAN_WINDOW
	};
	int err;

	LOG_DBG("");

	err = bt_le_scan_start(&scan_param, bt_mesh_scan_cb);
	if (err && err != -EALREADY) {
		LOG_ERR("starting scan failed (err %d)", err);
		return err;
	}

	return 0;
}

int bt_mesh_scan_disable(void)
{
	int err;

	LOG_DBG("");

	err = bt_le_scan_stop();
	if (err && err != -EALREADY) {
		LOG_ERR("stopping scan failed (err %d)", err);
		return err;
	}

	return 0;
}

void bt_mesh_adv_buf_init(void)
{
	bt_fifo_init(&bt_mesh_adv_queue);
	bt_fifo_init(&bt_mesh_relay_queue);
	bt_fifo_init(&bt_mesh_friend_queue);
	bt_mem_slab_init(&local_adv_pool);
#if defined(CONFIG_BT_MESH_RELAY_BUF_COUNT)
	bt_mem_slab_init(&relay_adv_pool);
#endif
#if defined(CONFIG_BT_MESH_ADV_EXT_FRIEND_SEPARATE)
	bt_mem_slab_init(&friend_adv_pool);
#endif
	bt_base_require(osi_sem_init(&adv_ready, 0, 1));
}
