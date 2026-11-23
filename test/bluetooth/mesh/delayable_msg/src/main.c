#include "support/test_support.h"
/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <embt/net_buf.h>
#include <embt/bluetooth/mesh.h>
#include <embt/random/random.h>

#include "net.h"
#include "access.h"
#include "delayable_msg.h"

#define SRC_ADDR 0x0002
#define RX_ADDR  0xc000

static void start_cb(uint16_t duration, int err, void *cb_data);

struct bt_mesh_net bt_mesh;

static struct bt_mesh_msg_ctx gctx = {.net_idx = 0,
				      .app_idx = 0,
				      .addr = 0,
				      .recv_dst = RX_ADDR,
				      .uuid = NULL,
				      .recv_rssi = 0,
				      .recv_ttl = 0x05,
				      .send_rel = false,
				      .rnd_delay = true,
				      .send_ttl = 0x06};

static struct bt_mesh_send_cb send_cb = {
	.start = start_cb,
};

static bool is_fake_random;
static bool check_expectations;
static bool accum_mask;
static bool do_not_call_cb;
static bool yield_in_send;
static uint16_t fake_random;
static uint8_t *buf_data;
static size_t buf_data_size;
static uint32_t expected_buf_id;
static uint16_t id_mask;
static int cb_err_status;
static bt_atomic_t send_call_count;

os_sem_t delayed_msg_sent;

/**** Mocked functions ****/
int bt_mesh_access_send(struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf, uint16_t src_addr,
			const struct bt_mesh_send_cb *cb, void *cb_data)
{
	bt_atomic_inc(&send_call_count);

	if (check_expectations) {
		gctx.rnd_delay = false;
		assert_memory_equal(ctx, &gctx, sizeof(*ctx));
		gctx.rnd_delay = true;
		assert_int_equal(src_addr, SRC_ADDR);
		assert_ptr_equal(cb, &send_cb);
		assert_memory_equal(cb_data, &expected_buf_id, sizeof(expected_buf_id));
		assert_memory_equal(buf->data, buf_data, buf_data_size);
	}

	if (cb && !do_not_call_cb) {
		cb->start(0x0, cb_err_status, cb_data);
	}

	/* Models the caller blocking inside the transport: the delayable msg has already been
	 * detached from busy_ctx at this point.
	 */
	if (yield_in_send) {
		osi_thread_yield();
	}

	return cb_err_status;
}

int bt_rand(void *buf, size_t len)
{
	if (is_fake_random) {
		*(uint16_t *)buf = fake_random;
	} else {
		sys_rand_get(buf, len);
	}
	return 0;
}
/**** Mocked functions ****/

static void start_cb(uint16_t duration, int err, void *cb_data)
{

	assert_int_equal(err, cb_err_status);

	if (accum_mask) {
		id_mask |= 1 << *(uint16_t *)cb_data;
	} else {
		osi_sem_give(&delayed_msg_sent);
	}
}

static void set_expectation(struct net_buf_simple *buf, uint32_t *buf_id)
{
	expected_buf_id = *buf_id;
	buf_data = buf->__buf;
	buf_data_size = buf->size;
	check_expectations = true;
}

static void tc_setup(void *fixture)
{
	is_fake_random = false;
	check_expectations = false;
	accum_mask = false;
	id_mask = 0;
	do_not_call_cb = false;
	yield_in_send = false;
	cb_err_status = 0;
	bt_atomic_set(&send_call_count, 0);
	osi_sem_reset(&delayed_msg_sent);
	/* The module only queues messages for a provisioned, running node. */
	bt_atomic_clear_bit(bt_mesh.flags, BT_MESH_SUSPENDED);
	bt_atomic_set_bit(bt_mesh.flags, BT_MESH_VALID);
	bt_mesh_delayable_msg_init();
}

static void tc_teardown(void *fixture)
{
	assert_int_equal(gctx.net_idx, 0);
	assert_int_equal(gctx.app_idx, 0);
	assert_int_equal(gctx.addr, 0);
	assert_int_equal(gctx.recv_dst, RX_ADDR);
	assert_null(gctx.uuid);
	assert_int_equal(gctx.recv_rssi, 0);
	assert_int_equal(gctx.recv_ttl, 0x05);
	assert_false(gctx.send_rel);
	assert_true(gctx.rnd_delay);
	assert_int_equal(gctx.send_ttl, 0x06);
}



/* Simple single message sending with full size.
 */
static void bt_mesh_delayable_msg__test_single_sending(void **cm_state)
{
	(void)cm_state;
	uint32_t buf_id = 0x55aa55aa;
	uint8_t *payload;

	NET_BUF_SIMPLE_DEFINE(buf, BT_MESH_TX_SDU_MAX);

	payload = net_buf_simple_add(&buf, BT_MESH_TX_SDU_MAX);
	for (int i = 0; i < BT_MESH_TX_SDU_MAX; i++) {
		payload[i] = i;
	}

	set_expectation(&buf, &buf_id);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id), 0);

	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);
}

/* The test checks that the delayed message mechanism sorts
 * the incoming messages according to the transmission start timestamp.
 */
static void bt_mesh_delayable_msg__test_self_sorting(void **cm_state)
{
	(void)cm_state;
	uint8_t tx_data[20];
	uint32_t buf1_id = 1;
	uint32_t buf2_id = 2;
	uint32_t buf3_id = 3;
	uint32_t buf4_id = 4;

	NET_BUF_SIMPLE_DEFINE(buf1, 20);
	NET_BUF_SIMPLE_DEFINE(buf2, 20);
	NET_BUF_SIMPLE_DEFINE(buf3, 20);
	NET_BUF_SIMPLE_DEFINE(buf4, 20);

	memset(tx_data, 1, 20);
	net_buf_simple_add_mem(&buf1, tx_data, 20);
	memset(tx_data, 2, 20);
	net_buf_simple_add_mem(&buf2, tx_data, 20);
	memset(tx_data, 3, 20);
	net_buf_simple_add_mem(&buf3, tx_data, 20);
	memset(tx_data, 4, 20);
	net_buf_simple_add_mem(&buf4, tx_data, 20);

	is_fake_random = true;
	fake_random = 30;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf1, SRC_ADDR, &send_cb, &buf1_id), 0);
	fake_random = 10;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf2, SRC_ADDR, &send_cb, &buf2_id), 0);
	fake_random = 20;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf3, SRC_ADDR, &send_cb, &buf3_id), 0);
	fake_random = 40;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf4, SRC_ADDR, &send_cb, &buf4_id), 0);

	set_expectation(&buf2, &buf2_id);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_MSEC(100)), 0);
	set_expectation(&buf3, &buf3_id);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_MSEC(100)), 0);
	set_expectation(&buf1, &buf1_id);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_MSEC(100)), 0);
	set_expectation(&buf4, &buf4_id);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_MSEC(100)), 0);
}

/* The test checks that the delayed msg mechanism can allocate new context
 * if all contexts are in use by sending the message, that is the closest to
 * the tx time.
 */
static void bt_mesh_delayable_msg__test_ctx_reallocation(void **cm_state)
{
	(void)cm_state;
	uint8_t tx_data[20];
	uint32_t buf_id1 = 0;
	uint32_t buf_id2 = 1;
	uint32_t buf_id3 = 2;
	uint32_t buf_id4 = 3;
	uint32_t buf_id5 = 4;

	NET_BUF_SIMPLE_DEFINE(buf, 20);

	memset(tx_data, 1, 20);
	net_buf_simple_add_mem(&buf, tx_data, 20);

	accum_mask = true;
	is_fake_random = true;
	fake_random = 10;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id1), 0);
	fake_random = 30;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id2), 0);
	fake_random = 20;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id3), 0);
	fake_random = 40;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id4), 0);
	fake_random = 40;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id5), 0);
	assert_int_equal(id_mask, 0x0001);
	osi_sleep_ms(BT_MSEC(500));
	assert_int_equal(id_mask, 0x001F);
}

/* The test checks that the delayed msg mechanism can allocate new chunks
 * if all chunks are in use by sending the other messages.
 */
static void bt_mesh_delayable_msg__test_chunk_reallocation(void **cm_state)
{
	(void)cm_state;
	uint8_t tx_data[BT_MESH_TX_SDU_MAX];
	uint32_t buf_id1 = 0;
	uint32_t buf_id2 = 1;
	uint32_t buf_id3 = 2;
	uint32_t buf_id4 = 3;

	NET_BUF_SIMPLE_DEFINE(buf1, 20);
	NET_BUF_SIMPLE_DEFINE(buf2, BT_MESH_TX_SDU_MAX);

	memset(tx_data, 1, BT_MESH_TX_SDU_MAX);
	net_buf_simple_add_mem(&buf2, tx_data, BT_MESH_TX_SDU_MAX);

	accum_mask = true;
	net_buf_simple_add_mem(&buf1, tx_data, 20);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf1, SRC_ADDR, &send_cb, &buf_id1), 0);
	net_buf_simple_reset(&buf1);
	net_buf_simple_add_mem(&buf1, tx_data, 20);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf1, SRC_ADDR, &send_cb, &buf_id2), 0);
	net_buf_simple_reset(&buf1);
	net_buf_simple_add_mem(&buf1, tx_data, 20);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf1, SRC_ADDR, &send_cb, &buf_id3), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf2, SRC_ADDR, &send_cb, &buf_id4), 0);
	assert_int_equal(id_mask, 0x0007);
	osi_sleep_ms(BT_MSEC(500));
	assert_int_equal(id_mask, 0x000F);
}

/* The test checks that the delayed msg mechanism can reschedule access messages
 * transport layes doesn't have enough memory or buffers at the moment.
 * Also it checks that the delayed msg mechanism can handle the other transport
 * layer errors without rescheduling appropriate access messages.
 */
static void bt_mesh_delayable_msg__test_cb_error_status(void **cm_state)
{
	(void)cm_state;
	uint32_t buf_id = 0x55aa55aa;
	uint8_t tx_data[BT_MESH_TX_SDU_MAX];

	NET_BUF_SIMPLE_DEFINE(buf1, 20);
	NET_BUF_SIMPLE_DEFINE(buf2, 20);
	NET_BUF_SIMPLE_DEFINE(buf3, 20);

	memset(tx_data, 1, BT_MESH_TX_SDU_MAX);
	net_buf_simple_add_mem(&buf1, tx_data, 20);
	net_buf_simple_add_mem(&buf2, tx_data, 20);
	net_buf_simple_add_mem(&buf3, tx_data, 20);

	cb_err_status = -ENOBUFS;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf1, SRC_ADDR, &send_cb, &buf_id), 0);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);
	cb_err_status = 0;
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);

	cb_err_status = -EBUSY;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf2, SRC_ADDR, &send_cb, &buf_id), 0);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);
	cb_err_status = 0;
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);

	cb_err_status = -EINVAL;
	do_not_call_cb = true;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf3, SRC_ADDR, &send_cb, &buf_id), 0);
	assert_int_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);
	cb_err_status = 0;
	assert_int_not_equal(osi_sem_take(&delayed_msg_sent, BT_SECONDS(1)), 0);
}

/* The test checks that the delayed msg mechanism raises
 * the model message callback with the appropriate error code after
 * stopping the functionality.
 */
static void bt_mesh_delayable_msg__test_stop_handler(void **cm_state)
{
	(void)cm_state;
	uint8_t tx_data[20];
	uint32_t buf_id1 = 0;
	uint32_t buf_id2 = 1;
	uint32_t buf_id3 = 2;
	uint32_t buf_id4 = 3;

	NET_BUF_SIMPLE_DEFINE(buf, 20);

	memset(tx_data, 1, 20);
	net_buf_simple_add_mem(&buf, tx_data, 20);

	accum_mask = true;
	cb_err_status = -ENODEV;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id1), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id2), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id3), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf, SRC_ADDR, &send_cb, &buf_id4), 0);
	bt_mesh_delayable_msg_stop();
	/* Long enough for any timer that survived the stop to fire. */
	osi_sleep_ms(BT_SECONDS(1));
	assert_int_equal(bt_atomic_get(&send_call_count), 0);
	assert_int_equal(id_mask, 0x000F);
}

/* The test checks that chunk allocation is based on actual data length (buf->len)
 * and not on buffer capacity (buf->size). Four messages with small payloads in
 * large-capacity buffers should consume exactly the full chunk pool without error
 * or eviction.
 *
 * With CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG_CHUNK_SIZE=20,
 * CONFIG_BT_MESH_ACCESS_DELAYABLE_MSG_CHUNK_COUNT=20 and data_len=100:
 *   DIV_ROUND_UP(100, 20) = 5 chunks per msg, 4 msgs * 5 = 20 chunks (full pool)
 */
static void bt_mesh_delayable_msg__test_chunk_alloc_by_data_len(void **cm_state)
{
	(void)cm_state;
	uint8_t tx_data[100];
	uint32_t buf_id1 = 0;
	uint32_t buf_id2 = 1;
	uint32_t buf_id3 = 2;
	uint32_t buf_id4 = 3;

	NET_BUF_SIMPLE_DEFINE(buf1, BT_MESH_TX_SDU_MAX);
	NET_BUF_SIMPLE_DEFINE(buf2, BT_MESH_TX_SDU_MAX);
	NET_BUF_SIMPLE_DEFINE(buf3, BT_MESH_TX_SDU_MAX);
	NET_BUF_SIMPLE_DEFINE(buf4, BT_MESH_TX_SDU_MAX);

	memset(tx_data, 1, sizeof(tx_data));
	net_buf_simple_add_mem(&buf1, tx_data, sizeof(tx_data));
	net_buf_simple_add_mem(&buf2, tx_data, sizeof(tx_data));
	net_buf_simple_add_mem(&buf3, tx_data, sizeof(tx_data));
	net_buf_simple_add_mem(&buf4, tx_data, sizeof(tx_data));

	accum_mask = true;
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf1, SRC_ADDR, &send_cb, &buf_id1), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf2, SRC_ADDR, &send_cb, &buf_id2), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf3, SRC_ADDR, &send_cb, &buf_id3), 0);
	assert_int_equal(bt_mesh_delayable_msg_manage(&gctx, &buf4, SRC_ADDR, &send_cb, &buf_id4), 0);

	/* All 4 messages should coexist using the full chunk pool (5 chunks each)
	 * without any eviction.
	 */
	assert_int_equal(id_mask, 0);

	osi_sleep_ms(BT_MSEC(500));
	assert_int_equal(id_mask, 0x000F);
}

/* These upstream cases require cooperative priority and non-preemptible
 * interleavings. POSIX OSI cannot reproduce those scheduling contracts.
 * Their source locations and prerequisites are recorded in EXCLUSIONS.md.
 */
static void bt_mesh_delayable_msg__test_concurrent_manage(void **cm_state)
{
 (void)cm_state;
 skip();
}

static void bt_mesh_delayable_msg__test_stop_during_in_flight_send(void **cm_state)
{
 (void)cm_state;
 skip();
}

static void bt_mesh_delayable_msg__test_stale_handler_no_early_send(void **cm_state)
{
 (void)cm_state;
 skip();
}

static void storage_init_1f144ecd32(void)
{
	bt_test_require(osi_sem_init(&delayed_msg_sent, 0, 1));
}

int bt_test_bt_mesh_delayable_msg_setup(void **state)
{
	int environment_result = bt_test_base_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	storage_init_1f144ecd32();
	*state = NULL;
	return 0;
}
static int bt_test_bt_mesh_delayable_msg_before(void **state)
{
	(void)state;
	tc_setup(*state);
	return 0;
}
int bt_test_bt_mesh_delayable_msg_after(void **state)
{
	(void)state;
	tc_teardown(*state);
	return 0;
}

static const struct CMUnitTest test_cases_bt_mesh_delayable_msg[] = {
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_single_sending, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_self_sorting, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_ctx_reallocation, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_chunk_reallocation, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_cb_error_status, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_stop_handler, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_chunk_alloc_by_data_len, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_concurrent_manage, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_stop_during_in_flight_send, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
	cmocka_unit_test_setup_teardown(bt_mesh_delayable_msg__test_stale_handler_no_early_send, bt_test_bt_mesh_delayable_msg_before, bt_test_bt_mesh_delayable_msg_after),
};
BT_TEST_SUITE(bt_mesh_delayable_msg, test_cases_bt_mesh_delayable_msg, bt_test_bt_mesh_delayable_msg_setup, NULL);
