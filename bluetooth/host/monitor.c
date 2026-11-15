/** @file
 *  @brief Custom logging over UART
 */

/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/osi/thread.h>
#include <embt/base/spinlock.h>
#include <embt/base/time.h>
#include <embt/base/assert.h>
#include <embt/base/util_macro.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/buf.h>
#include <embt/base/atomic.h>
#include <embt/base/byteorder.h>
#include <embt/base/time.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>

#include "monitor.h"
#include "monitor_buffer.h"

/* This is the same default priority as for other console handlers,
 * except that we're not exporting it as a Kconfig variable until a
 * clear need arises.
 */
#define MONITOR_INIT_PRIORITY 60

/* These defines follow the values used by syslog(2) */
#define BT_LOG_ERR      3
#define BT_LOG_WARN     4
#define BT_LOG_INFO     6
#define BT_LOG_DBG      7

/* TS resolution is 1/10th of a millisecond */
#define MONITOR_TS_FREQ 10000

/* Maximum (string) length of a log message */
#define MONITOR_MSG_MAX 128

enum {
	BT_LOG_BUSY,
	BT_CONSOLE_BUSY,
};

static bt_atomic_t flags;

static struct {
	bt_atomic_t cmd;
	bt_atomic_t evt;
	bt_atomic_t acl_tx;
	bt_atomic_t acl_rx;
#if defined(CONFIG_BT_CLASSIC)
	bt_atomic_t sco_tx;
	bt_atomic_t sco_rx;
#endif
	bt_atomic_t other;
} drops;

static void drop_add(uint16_t opcode)
{
	switch (opcode) {
	case BT_MONITOR_COMMAND_PKT:
		bt_atomic_inc(&drops.cmd);
		break;
	case BT_MONITOR_EVENT_PKT:
		bt_atomic_inc(&drops.evt);
		break;
	case BT_MONITOR_ACL_TX_PKT:
		bt_atomic_inc(&drops.acl_tx);
		break;
	case BT_MONITOR_ACL_RX_PKT:
		bt_atomic_inc(&drops.acl_rx);
		break;
#if defined(CONFIG_BT_CLASSIC)
	case BT_MONITOR_SCO_TX_PKT:
		bt_atomic_inc(&drops.sco_tx);
		break;
	case BT_MONITOR_SCO_RX_PKT:
		bt_atomic_inc(&drops.sco_rx);
		break;
#endif
	default:
		bt_atomic_inc(&drops.other);
		break;
	}
}

static bt_monitor_sink_t monitor_sink;
static void *monitor_sink_context;

int bt_monitor_init(bt_monitor_sink_t sink, void *context)
{
	if (!sink) {
		return -EINVAL;
	}
	monitor_sink = sink;
	monitor_sink_context = context;
	return 0;
}

static bool monitor_send(const struct bt_monitor_data *frags, size_t count)
{
	return monitor_sink && !monitor_sink(frags, count, monitor_sink_context);
}

static void encode_drops(struct bt_monitor_hdr *hdr, uint8_t type,
			 bt_atomic_t *val)
{
	bt_atomic_val_t count;

	count = bt_atomic_set(val, 0);
	if (count) {
		hdr->ext[hdr->hdr_len++] = type;
		hdr->ext[hdr->hdr_len++] = MIN(count, 255);
		if (count > 255) {
			/* Keep the surplus for the next record */
			bt_atomic_add(val, count - 255);
		}
	}
}

static bt_atomic_t *drop_counter(uint8_t type)
{
	switch (type) {
	case BT_MONITOR_COMMAND_DROPS:
		return &drops.cmd;
	case BT_MONITOR_EVENT_DROPS:
		return &drops.evt;
	case BT_MONITOR_ACL_TX_DROPS:
		return &drops.acl_tx;
	case BT_MONITOR_ACL_RX_DROPS:
		return &drops.acl_rx;
#if defined(CONFIG_BT_CLASSIC)
	case BT_MONITOR_SCO_TX_DROPS:
		return &drops.sco_tx;
	case BT_MONITOR_SCO_RX_DROPS:
		return &drops.sco_rx;
#endif
	case BT_MONITOR_OTHER_DROPS:
		return &drops.other;
	default:
		return NULL;
	}
}

/* Restore drop counts that encode_hdr() consumed into a record which then
 * failed to be sent: without this, every record dropped back-to-back would
 * destroy the counts accumulated by its predecessors, understating drops
 * under sustained overload.
 */
static void restore_drops(const struct bt_monitor_hdr *hdr)
{
	/* The timestamp is always encoded first, drop pairs follow */
	for (uint8_t i = sizeof(struct bt_monitor_ts32); i + 1U < hdr->hdr_len; i += 2U) {
		bt_atomic_t *counter = drop_counter(hdr->ext[i]);

		if (counter != NULL) {
			bt_atomic_add(counter, hdr->ext[i + 1]);
		}
	}
}

/* Convert microseconds to the monitor timestamp resolution */
static uint64_t monitor_ts_from_us(uint64_t us)
{
	return us / (USEC_PER_SEC / MONITOR_TS_FREQ);
}

/* HCI packet timestamps use the OSI monotonic microsecond origin. */
static uint64_t monitor_ts_get(void)
{
	return monitor_ts_from_us(bt_uptime_get_us());
}

static inline void encode_hdr(struct bt_monitor_hdr *hdr, uint64_t timestamp,
			      uint16_t opcode, uint16_t len)
{
	struct bt_monitor_ts32 *ts;

	hdr->opcode   = sys_cpu_to_le16(opcode);
	hdr->flags    = 0U;

	ts = (void *)hdr->ext;
	ts->type = BT_MONITOR_TS32;
	/* The btsnoop protocol only supports 32-bit timestamps (in 1/10th ms).
	 * This overflows after 4.97 days, which is acceptable as it only affects
	 * rendering in btmon/wireshark. Recordings are usually not that long
	 * anyway, and packet ordering is still preserved.
	 */
	ts->ts32 = (uint32_t)timestamp;
	hdr->hdr_len = sizeof(*ts);

	encode_drops(hdr, BT_MONITOR_COMMAND_DROPS, &drops.cmd);
	encode_drops(hdr, BT_MONITOR_EVENT_DROPS, &drops.evt);
	encode_drops(hdr, BT_MONITOR_ACL_TX_DROPS, &drops.acl_tx);
	encode_drops(hdr, BT_MONITOR_ACL_RX_DROPS, &drops.acl_rx);
#if defined(CONFIG_BT_CLASSIC)
	encode_drops(hdr, BT_MONITOR_SCO_TX_DROPS, &drops.sco_tx);
	encode_drops(hdr, BT_MONITOR_SCO_RX_DROPS, &drops.sco_rx);
#endif
	encode_drops(hdr, BT_MONITOR_OTHER_DROPS, &drops.other);

	hdr->data_len = sys_cpu_to_le16(4 + hdr->hdr_len + len);
}

void bt_monitor_send(uint16_t opcode, const void *data, size_t len)
{
	struct bt_monitor_hdr hdr;
	struct bt_monitor_data frags[] = {
		{ &hdr, 0 }, /* Length known only after encode_hdr() */
		{ data, len },
	};

	if (bt_atomic_test_and_set_bit(&flags, BT_LOG_BUSY)) {
		drop_add(opcode);
		return;
	}

	encode_hdr(&hdr, monitor_ts_get(), opcode, len);
	frags[0].len = BT_MONITOR_BASE_HDR_LEN + hdr.hdr_len;

	if (!monitor_send(frags, ARRAY_SIZE(frags))) {
		restore_drops(&hdr);
		drop_add(opcode);
	}

	bt_atomic_clear_bit(&flags, BT_LOG_BUSY);
}

void bt_monitor_new_index(uint8_t type, uint8_t bus, const bt_addr_t *addr,
			  const char *name)
{
	struct bt_monitor_new_index pkt;

	pkt.type = type;
	pkt.bus = bus;
	memcpy(pkt.bdaddr, addr, 6);
	strncpy(pkt.name, name, sizeof(pkt.name) - 1);
	pkt.name[sizeof(pkt.name) - 1] = '\0';

	bt_monitor_send(BT_MONITOR_NEW_INDEX, &pkt, sizeof(pkt));
}

int bt_monitor_console_out(int c)
{
	static char buf[MONITOR_MSG_MAX];
	static size_t len;

	if (bt_atomic_test_and_set_bit(&flags, BT_CONSOLE_BUSY)) {
		return c;
	}

	if (c != '\n' && len < sizeof(buf) - 1) {
		buf[len++] = c;
		bt_atomic_clear_bit(&flags, BT_CONSOLE_BUSY);
		return c;
	}

	buf[len++] = '\0';

	bt_monitor_send(BT_MONITOR_SYSTEM_NOTE, buf, len);
	len = 0;

	bt_atomic_clear_bit(&flags, BT_CONSOLE_BUSY);

	return c;
}
