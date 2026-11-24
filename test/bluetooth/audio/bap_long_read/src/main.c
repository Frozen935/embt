/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

/** @file @brief Exercise the real BAP reader on the real Host queue with GATT provider fakes. */
#include "support/test_support.h"

/* Rename only external providers in this translation unit. Production scheduling is unchanged. */
#define bt_conn_index pac_test_conn_index
#define bt_conn_lookup_index pac_test_conn_lookup_index
#define bt_conn_ref pac_test_conn_ref
#define bt_conn_unref pac_test_conn_unref
#define bt_conn_get_info pac_test_conn_get_info
#define bt_conn_is_type pac_test_conn_is_type
#define bt_gatt_read pac_test_gatt_read
#define bt_gatt_subscribe pac_test_gatt_subscribe
#define bt_gatt_discover pac_test_gatt_discover
#define bt_att_get_max_notify_size pac_test_notify_size
#define bt_audio_security_check pac_test_security_check
#include "../../../../../bluetooth/audio/bap_unicast_client.c"

static struct bt_conn connection;
static struct unicast_client *client = &uni_cli_insts[0];
static unsigned int references;
static unsigned int reads;
static uint16_t handles[16];
static uint64_t read_times[16];
static struct bt_gatt_read_params *request;
static int read_error;
static int subscribe_error;
static int action_result;
static bool callback_storage_intact;
static bool reenter_discovery;
static unsigned int records;
static enum bt_audio_dir record_dir;
static os_sem_t read_started;
static os_sem_t action_done;
static struct bt_work action_work;
static void (*owner_action)(void);

uint8_t pac_test_conn_index(const struct bt_conn *conn)
{
	(void)conn;
	return 0;
}

struct bt_conn *pac_test_conn_lookup_index(uint8_t index)
{
	(void)index;
	references++;
	return &connection;
}

struct bt_conn *pac_test_conn_ref(struct bt_conn *conn)
{
	references++;
	return conn;
}

void pac_test_conn_unref(struct bt_conn *conn)
{
	(void)conn;
	if (references == 0) {
		abort();
	}
	references--;
}

int pac_test_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info)
{
	(void)conn;
	memset(info, 0, sizeof(*info));
	info->role = BT_CONN_ROLE_CENTRAL;
	info->le.interval_us = 50000;
	return 0;
}

bool pac_test_conn_is_type(const struct bt_conn *conn, enum bt_conn_type type)
{
	(void)conn;
	return type == BT_CONN_TYPE_LE;
}

int pac_test_notify_size(struct bt_conn *conn, enum bt_att_chan_opt option)
{
	(void)conn;
	(void)option;
	return 20;
}

uint8_t pac_test_security_check(const struct bt_conn *conn)
{
	(void)conn;
	return BT_ATT_ERR_SUCCESS;
}

int pac_test_gatt_read(struct bt_conn *conn, struct bt_gatt_read_params *params)
{
	int result = read_error;

	(void)conn;
	if (!bt_is_work_thread() || reads >= ARRAY_SIZE(handles) ||
	    params != &client->long_read_params || params->single.offset != 0) {
		abort();
	}
	handles[reads] = params->single.handle;
	bt_test_require(osi_time_get_ms(&read_times[reads]));
	reads++;
	request = result == 0 ? params : NULL;
	read_error = 0;
	bt_test_require(osi_sem_give(&read_started));
	return result;
}

int pac_test_gatt_subscribe(struct bt_conn *conn, struct bt_gatt_subscribe_params *params)
{
	(void)conn;
	(void)params;
	return subscribe_error;
}

int pac_test_gatt_discover(struct bt_conn *conn, struct bt_gatt_discover_params *params)
{
	(void)conn;
	(void)params;
	return 0;
}

static void action_handler(struct bt_work *work)
{
	(void)work;
	owner_action();
	bt_test_require(osi_sem_give(&action_done));
}

/** @brief Join a real Host action, including the worker's post-handler updates. */
static void on_owner(void (*action)(void))
{
	struct bt_work_sync sync = {0};

	owner_action = action;
	assert_true(bt_host_work_submit(&action_work) >= 0);
	assert_int_equal(osi_sem_take(&action_done, 2000), 0);
	(void)bt_work_flush(&action_work, &sync);
}

static void no_action(void)
{
}

static void wait_for_read(void)
{
	assert_int_equal(osi_sem_take(&read_started, 2000), 0);
	on_owner(no_action);
}

static void notify_sink(void)
{
	uint8_t value[20] = {0};

	(void)unicast_client_pac_notify_cb(&connection, &client->snk_pac_subscribe,
					 value, sizeof(value));
}

static void finish_read(void)
{
	const uint8_t empty_pac[] = {0};
	struct bt_gatt_read_params *params = request;

	if (params == NULL) {
		abort();
	}
	if (params->single.handle == 0x30) {
		action_result = params->func(&connection, BT_ATT_ERR_UNLIKELY, params, NULL, 0);
	} else {
		action_result = params->func(&connection, 0, params, empty_pac, sizeof(empty_pac));
		if (action_result == BT_GATT_ITER_CONTINUE) {
			action_result = params->func(&connection, 0, params, NULL, 0);
		}
	}
	request = NULL;
}

static void complete_write(void)
{
	gatt_write_cb(&connection, 0, &client->write_params);
}

static void busy_write(void)
{
	bt_atomic_set_bit(client->flags, UNICAST_CLIENT_FLAG_BUSY);
	client->write_params.handle = 0x99;
	client->write_params.data = client->att_buf;
	net_buf_simple_add_u8(&client->net_buf, 0x5a);
	notify_sink();
}

static void pending_while_write(void **state)
{
	(void)state;
	on_owner(busy_write);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_int_equal(reads, 0);
	assert_int_equal(client->write_params.handle, 0x99);
	assert_ptr_equal(client->write_params.data, client->att_buf);
	assert_int_equal(client->net_buf.len, 1);
	assert_int_equal(client->net_buf.data[0], 0x5a);
	on_owner(complete_write);
	wait_for_read();
	assert_ptr_equal(request, &client->long_read_params);
	assert_int_equal(handles[0], 0x10);
	on_owner(finish_read);
	assert_int_equal(action_result, BT_GATT_ITER_STOP);
	assert_int_equal(references, 0);
}

static void supersede_and_queue(void)
{
	uint8_t short_value[] = {0};
	uint8_t full_value[20] = {0};
	bt_gatt_read_func_t callback = request->func;

	(void)unicast_client_pac_notify_cb(&connection, &client->snk_pac_subscribe,
					 short_value, sizeof(short_value));
	(void)unicast_client_pac_notify_cb(&connection, &client->src_pac_subscribe,
					 full_value, sizeof(full_value));
	(void)unicast_client_check_and_do_long_read(&connection, 0x30, full_value,
						 sizeof(full_value), unicast_client_ase_ntf_read_func);
	callback_storage_intact = request->func == callback && request->single.handle == 0x10;
	action_result = callback(&connection, 0, request, full_value, sizeof(full_value));
	request = NULL;
}

static void superseding_handles_are_fair(void **state)
{
	(void)state;
	on_owner(notify_sink);
	wait_for_read();
	on_owner(supersede_and_queue);
	assert_true(callback_storage_intact);
	assert_int_equal(action_result, BT_GATT_ITER_STOP);
	wait_for_read();
	assert_int_equal(handles[1], 0x20);
	on_owner(finish_read);
	wait_for_read();
	assert_int_equal(handles[2], 0x30);
	on_owner(finish_read);
	wait_for_read();
	assert_int_equal(handles[3], 0x10);
	on_owner(finish_read);
	assert_int_equal(references, 0);
}

static void off_owner_removal_retains_read(void **state)
{
	bt_gatt_read_func_t callback;

	(void)state;
	on_owner(notify_sink);
	wait_for_read();
	callback = request->func;
	(void)unicast_client_pac_notify_cb(NULL, &client->snk_pac_subscribe, NULL, 0);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_ptr_equal(request->func, callback);
	assert_int_equal(request->single.handle, 0x10);
	assert_true(client->long_read_active);
	on_owner(finish_read);
	assert_int_equal(action_result, BT_GATT_ITER_STOP);
	assert_int_equal(reads, 1);
	assert_int_equal(references, 0);
}

static void subscribe_sink(void)
{
	client->dir = BT_AUDIO_DIR_SINK;
	action_result = unicast_client_pac_subscribe(&connection, 0x10);
}

static void complete_subscribe(void)
{
	struct bt_gatt_subscribe_params *params = &client->snk_pac_subscribe;
	bt_gatt_notify_func_t notify = params->notify;

	params->subscribe(&connection, 0, params);
	callback_storage_intact = params->value_handle == 0x10 && params->notify == notify &&
		params->value == BT_GATT_CCC_NOTIFY;
}

static void ccc_removal_waits_for_completion(void **state)
{
	(void)state;
	client->snk_pac_subscribe.value_handle = 0;
	on_owner(subscribe_sink);
	assert_int_equal(action_result, 0);
	assert_true(client->pac_subscribe_pending[0]);
	(void)unicast_client_pac_notify_cb(NULL, &client->snk_pac_subscribe, NULL, 0);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_int_equal(client->snk_pac_subscribe.value_handle, 0x10);
	on_owner(subscribe_sink);
	assert_int_equal(action_result, -EBUSY);
	on_owner(complete_subscribe);
	assert_true(callback_storage_intact);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_int_equal(client->snk_pac_subscribe.value_handle, 0);
	on_owner(subscribe_sink);
	assert_int_equal(action_result, 0);
	assert_non_null(client->snk_pac_subscribe.disc_params);
}

static void ccc_not_found(void)
{
	(void)unicast_client_pac_notify_cb(&connection, &client->snk_pac_subscribe, NULL, 0);
	callback_storage_intact = client->snk_pac_subscribe.value_handle == 0x10;
}

static void ccc_missing_descriptor_retires_pending(void **state)
{
	(void)state;
	client->snk_pac_subscribe.value_handle = 0;
	on_owner(subscribe_sink);
	on_owner(ccc_not_found);
	assert_true(callback_storage_intact);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_false(client->pac_subscribe_pending[0]);
	assert_int_equal(client->snk_pac_subscribe.value_handle, 0);
}

static void disconnected(void)
{
	unicast_client_disconnected(&connection, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

static void synchronous_ccc_errors_and_teardown(void **state)
{
	const int errors[] = {-ENOMEM, -ENOTCONN, -EBUSY, -EALREADY};
	bt_work_handler_t handler = client->long_read_work.work.handler;

	(void)state;
	for (size_t i = 0; i < ARRAY_SIZE(errors); i++) {
		client->snk_pac_subscribe.value_handle = 0;
		subscribe_error = errors[i];
		on_owner(subscribe_sink);
		assert_int_equal(action_result, errors[i]);
		if (i < 2) {
			assert_false(client->pac_subscribe_pending[0]);
			assert_int_equal(client->snk_pac_subscribe.value_handle, 0);
		} else {
			assert_true(client->pac_subscribe_pending[0]);
			assert_int_equal(client->snk_pac_subscribe.value_handle, 0x10);
		}
		on_owner(disconnected);
		assert_int_equal(bt_host_work_drain(), 0);
		assert_false(client->pac_subscribe_pending[0]);
		assert_int_equal(client->snk_pac_subscribe.value_handle, 0);
		assert_ptr_equal(client->long_read_work.work.handler, handler);
		bt_atomic_clear(&client->long_read_stopping);
	}
}

static void pac_record(struct bt_conn *conn, enum bt_audio_dir dir,
		       const struct bt_audio_codec_cap *cap)
{
	(void)cap;
	records++;
	record_dir = dir;
	if (reenter_discovery) {
		action_result = bt_bap_unicast_client_discover(conn, BT_AUDIO_DIR_SOURCE);
	}
}

static void complete_one_record(void)
{
	const uint8_t pac[] = {1, BT_HCI_CODING_FORMAT_LC3, 0, 0, 0, 0, 0, 0};
	struct bt_gatt_read_params *params = request;

	(void)params->func(&connection, 0, params, pac, sizeof(pac));
	(void)params->func(&connection, 0, params, NULL, 0);
	request = NULL;
}

static void reentrant_discovery_keeps_its_busy(void **state)
{
	(void)state;
	reenter_discovery = true;
	on_owner(notify_sink);
	wait_for_read();
	on_owner(complete_one_record);
	assert_int_equal(records, 1);
	assert_int_equal(record_dir, BT_AUDIO_DIR_SINK);
	assert_int_equal(action_result, 0);
	assert_true(bt_atomic_test_bit(client->flags, UNICAST_CLIENT_FLAG_BUSY));
	assert_true(bt_atomic_test_bit(client->flags, UNICAST_CLIENT_FLAG_DISCOVERY_IN_PROGRESS));
	assert_false(client->long_read_owns_busy);
	assert_false(client->long_read_active);
	assert_int_equal(references, 0);
}

static void read_allocation_retry_is_delayed(void **state)
{
	(void)state;
	read_error = -ENOMEM;
	on_owner(notify_sink);
	wait_for_read();
	wait_for_read();
	assert_int_equal(reads, 2);
	assert_true(read_times[1] - read_times[0] >= 50);
	on_owner(finish_read);
}

static struct bt_bap_unicast_client_cb listener = {.pac_record = pac_record};

static int group_setup(void **state)
{
	int result = bt_test_host_setup(state);

	if (result != 0) {
		return result;
	}
	bt_test_require(osi_sem_init(&read_started, 0, 16));
	bt_test_require(osi_sem_init(&action_done, 0, 1));
	bt_work_init(&action_work, action_handler);
	long_read_initialize(client);
	return bt_bap_unicast_client_register_cb(&listener);
}

static int case_setup(void **state)
{
	(void)state;
	reads = records = references = 0;
	request = NULL;
	read_error = subscribe_error = action_result = 0;
	reenter_discovery = callback_storage_intact = false;
	bt_atomic_clear(&client->long_read_stopping);
	bt_atomic_clear(client->flags);
	for (size_t slot = 0; slot < BAP_LONG_READ_SLOTS; slot++) {
		bt_atomic_clear_bit(client->long_read_pending, slot);
		bt_atomic_clear_bit(client->long_read_removed, slot);
	}
	client->long_read_next = 0;
	client->pac_subscribe_pending[0] = client->pac_subscribe_pending[1] = false;
	client->snk_pac_subscribe.value_handle = 0x10;
	client->src_pac_subscribe.value_handle = 0x20;
#if CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT > 0
	client->snks[0].subscribe.value_handle = 0x30;
	client->snks[0].handle = 0x30;
#else
	client->srcs[0].subscribe.value_handle = 0x30;
	client->srcs[0].handle = 0x30;
#endif
	reset_att_buf(client);
	return 0;
}

static int case_teardown(void **state)
{
	struct bt_work_sync sync = {0};

	(void)state;
	bt_atomic_set(&client->long_read_stopping, 1);
	if (request != NULL) {
		on_owner(finish_read);
	}
	on_owner(disconnected);
	(void)bt_work_cancel_delayable_sync(&client->long_read_work, &sync);
	assert_int_equal(bt_host_work_drain(), 0);
	assert_int_equal(references, 0);
	return 0;
}

static const struct CMUnitTest cases[] = {
	cmocka_unit_test_setup_teardown(pending_while_write, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(superseding_handles_are_fair, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(off_owner_removal_retains_read, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(ccc_removal_waits_for_completion, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(ccc_missing_descriptor_retires_pending, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(synchronous_ccc_errors_and_teardown, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(reentrant_discovery_keeps_its_busy, case_setup, case_teardown),
	cmocka_unit_test_setup_teardown(read_allocation_retry_is_delayed, case_setup, case_teardown),
};
BT_TEST_SUITE(bap_long_read, cases, group_setup, NULL);
