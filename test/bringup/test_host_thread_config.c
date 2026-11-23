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

/** @file @brief Verify actual Host worker parameters/order and configured RTP wire version. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <embt/osi/thread.h>
#include <embt/bluetooth/classic/a2dp.h>
#include <embt/net_buf.h>
#include "host/classic/avdtp_internal.h"
#include "common/bt_shell_private.h"
#include <errno.h>
#include <string.h>

struct recorded_thread {
	char name[32];
	size_t stack_size;
	int priority;
};

static struct recorded_thread records[32];
static size_t record_count;
static unsigned int sent_version;

int __real_osi_thread_create(os_thread_t *thread, void (*entry)(void *), void *argument,
			     const char *name, int priority, size_t stack_size);

/**
 * @brief Record parameters while creating the real native worker.
 * @param[out] thread Native handle.
 * @param[in] entry Worker routine.
 * @param[in,out] argument Worker context.
 * @param[in] name Diagnostic name.
 * @param[in] priority Logical priority offset.
 * @param[in] stack_size Requested byte count.
 * @return The real OSI creation result.
 */
int __wrap_osi_thread_create(os_thread_t *thread, void (*entry)(void *), void *argument,
			     const char *name, int priority, size_t stack_size)
{
	if (record_count < sizeof(records) / sizeof(records[0])) {
		struct recorded_thread *record = &records[record_count++];
		strncpy(record->name, name ? name : "", sizeof(record->name) - 1);
		record->stack_size = stack_size;
		record->priority = priority;
	}
	return __real_osi_thread_create(thread, entry, argument, name, priority, stack_size);
}

/**
 * @brief Observe the real serialized RTP header before transport submission.
 * @param[in] sep Borrowed endpoint, unused by this transport interception.
 * @param[in] buf Actual serialized media buffer.
 * @return -EAGAIN to verify transport errors remain observable.
 */
int __wrap_bt_avdtp_send_media_data(struct bt_avdtp_sep *sep, struct net_buf *buf)
{
	(void)sep;
	sent_version = buf->data[0] >> 6;
	return -EAGAIN;
}

/**
 * @brief Verify a named worker receives its configured parameters.
 * @param[in] name Expected worker name.
 * @param[in] stack_size Expected byte count.
 * @param[in] priority Expected original priority number.
 * @return Position in the actual creation sequence.
 */
static size_t worker_index(const char *name, size_t stack_size, int priority)
{
	for (size_t i = 0; i < record_count; i++) {
		if (!strcmp(records[i].name, name)) {
			assert_int_equal(records[i].stack_size, stack_size);
			assert_int_equal(records[i].priority, priority);
			return i;
		}
	}
	fail_msg("Missing real worker %s", name);
	return 0;
}

/** @brief Every configured queue must start with the actual custom byte count and sorted order. */
static void host_workers(void **state)
{
	(void)state;
	assert_int_equal(bt_stack_init(), 0);
	worker_index("BT timeout", CONFIG_EMBT_TIMEOUT_THREAD_STACK_SIZE,
		CONFIG_EMBT_TIMEOUT_THREAD_PRIO);
	worker_index("embt-system", CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE,
		CONFIG_SYSTEM_WORKQUEUE_PRIORITY);
	const size_t rx = worker_index("BT RX WQ",
		CONFIG_BT_RX_STACK_SIZE, CONFIG_BT_RX_PRIO);
	const size_t tx = worker_index("BT TX",
		CONFIG_BT_TX_PROCESSOR_STACK_SIZE,
		CONFIG_BT_TX_PROCESSOR_THREAD_PRIO);
	const size_t lw = worker_index("BT LW WQ", CONFIG_BT_LONG_WQ_STACK_SIZE,
		CONFIG_BT_LONG_WQ_PRIO);
	const size_t notify = worker_index("BT conn TX", CONFIG_BT_CONN_TX_NOTIFY_WQ_STACK_SIZE,
		CONFIG_BT_CONN_TX_NOTIFY_WQ_PRIO);
	if (CONFIG_BT_CONN_TX_NOTIFY_WQ_INIT_PRIORITY < CONFIG_BT_RX_INIT_PRIO) {
		assert_true(notify < rx);
	}
	if (CONFIG_BT_TX_PROCESSOR_INIT_PRIO < CONFIG_BT_LONG_WQ_INIT_PRIO) {
		assert_true(tx < lw);
	}
	assert_true(worker_index("embt-system", CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE,
		CONFIG_SYSTEM_WORKQUEUE_PRIORITY) < notify);
}

/** @brief The configured two-bit value must reach the emitted RTP byte. */
static void rtp_configuration(void **state)
{
	uint8_t data[32];
	struct net_buf buffer = {.data = data, .len = 12, .size = sizeof(data)};
	struct bt_a2dp_ep endpoint = {.codec_type = BT_A2DP_SBC};
	struct bt_a2dp_stream stream = {.local_ep = &endpoint};
	(void)state;
	assert_int_equal(bt_a2dp_stream_send(&stream, &buffer, 1, 128), -EAGAIN);
	assert_int_equal(sent_version, CONFIG_BT_AVDTP_RTP_VERSION);
}

/** @brief Run real initialization without enabling HCI or wireless traffic. */
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(host_workers),
		cmocka_unit_test(rtp_configuration),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
