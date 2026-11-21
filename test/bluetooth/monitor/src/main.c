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

/**
 * @file
 * @brief Real monitor encoding with bounded memory and file output sinks.
 */

/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "support/test_support.h"
#include <embt/base/byteorder.h>
#include "monitor.h"
#include "monitor_buffer.h"

static uint8_t monitor_storage[128];
static struct ring_buf monitor_buf;
static bool reject_output;
static FILE *output_file;

/**
 * @brief Copy a complete record into the bounded capture queue.
 * @param[in] frags Borrowed header and payload fragments.
 * @param[in] count Number of fragments.
 * @param[in,out] context Capture ring buffer.
 * @retval 0 The complete record was copied.
 * @retval -ENOSPC No bytes were published.
 */
static int buffer_sink(const struct bt_monitor_data *frags, size_t count, void *context)
{
	if (reject_output || !bt_monitor_ring_buf_put(context, frags, count)) {
		return -ENOSPC;
	}
	return 0;
}

/**
 * @brief Write borrowed fragments synchronously to the test's temporary file.
 * @param[in] frags Borrowed header and payload fragments.
 * @param[in] count Number of fragments.
 * @param[in,out] context Open binary output file.
 * @retval 0 Every fragment was written.
 * @retval -EIO File output failed; previously written bytes are not rolled back.
 */
static int file_sink(const struct bt_monitor_data *frags, size_t count, void *context)
{
	FILE *file = context;

	for (size_t i = 0; i < count; i++) {
		if (frags[i].len && fwrite(frags[i].data, 1, frags[i].len, file) != frags[i].len) {
			return -EIO;
		}
	}
	return 0;
}

/* Large enough for every test's full capture */
static uint8_t stream[4096];
static size_t stream_len;

/* Payload: 4-byte little-endian sequence number followed by a pattern */
#define TEST_PAYLOAD_LEN 26
#define TEST_OPCODE      BT_MONITOR_ACL_TX_PKT

/* Payload fill pattern after the sequence number: cycles through the
 * uppercase alphabet.
 */
#define PATTERN_BYTE(idx) ('A' + ((idx) % 26))

static uint32_t tx_seq;

static void send_record_opcode(uint16_t opcode)
{
	uint8_t payload[TEST_PAYLOAD_LEN];

	sys_put_le32(tx_seq, payload);
	for (size_t i = sizeof(tx_seq); i < sizeof(payload); i++) {
		payload[i] = PATTERN_BYTE(i);
	}

	bt_monitor_send(opcode, payload, sizeof(payload));
	tx_seq++;
}

static void send_record(void)
{
	send_record_opcode(TEST_OPCODE);
}

/** @brief Drain committed bytes in small chunks, including wrapped records. */
static void capture(void)
{
	uint8_t *data;
	uint32_t len;

	while ((len = bt_monitor_ring_buf_get_ptr(&monitor_buf, &data)) != 0) {
		len = MIN(len, 7U);
		assert_true(len <= sizeof(stream) - stream_len);
		memcpy(stream + stream_len, data, len);
		stream_len += len;
		bt_monitor_ring_buf_consume(&monitor_buf, len);
	}
}

struct stream_stats {
	uint32_t records;      /* Well-formed payload records. */
	uint32_t last_seq;     /* sequence number of the last such record */
	uint32_t seq_errors;   /* out-of-order or corrupt payloads */
	uint32_t acl_tx_drops; /* accumulated from extended headers */
	uint32_t parse_errors; /* framing violations */
	uint32_t opcode_mask;
	uint8_t largest_acl_tx_drop;
};

static void parse_stream(struct stream_stats *st)
{
	size_t pos = 0;
	uint32_t expect_seq = 0;
	bool have_seq = false;

	(void)memset(st, 0, sizeof(*st));

	while (pos + BT_MONITOR_BASE_HDR_LEN <= stream_len) {
		/* data_len counts everything after the data_len field itself,
		 * i.e. the rest of the base header, the extended header and
		 * the payload.
		 */
		uint16_t data_len = sys_get_le16(&stream[pos]);
		uint16_t opcode =
			sys_get_le16(&stream[pos + offsetof(struct bt_monitor_hdr, opcode)]);
		uint8_t hdr_len = stream[pos + offsetof(struct bt_monitor_hdr, hdr_len)];
		size_t payload_len;
		const uint8_t *ext = &stream[pos + BT_MONITOR_BASE_HDR_LEN];
		const uint8_t *payload;
		size_t i = 0;

		/* Opcodes above the highest defined one indicate lost framing */
		if (data_len < BT_MONITOR_BASE_HDR_LEN - sizeof(data_len) + hdr_len ||
		    opcode > BT_MONITOR_ISO_RX_PKT ||
		    hdr_len < sizeof(struct bt_monitor_ts32) || hdr_len > BT_MONITOR_EXT_HDR_MAX ||
		    stream[pos + offsetof(struct bt_monitor_hdr, flags)] != 0) {
			st->parse_errors++;
			return;
		}
		if (pos + sizeof(data_len) + data_len > stream_len) {
			/* capture() drains the queue, so a partial tail is a framing error. */
			st->parse_errors++;
			return;
		}
		if (ext[0] != BT_MONITOR_TS32) {
			st->parse_errors++;
			return;
		}

		payload_len = data_len - (BT_MONITOR_BASE_HDR_LEN - sizeof(data_len)) - hdr_len;
		payload = &stream[pos + BT_MONITOR_BASE_HDR_LEN + hdr_len];

		while (i < hdr_len) {
			size_t field_len = ext[i] == BT_MONITOR_TS32 ?
					   sizeof(struct bt_monitor_ts32) : 2;

			if (field_len > hdr_len - i) {
				st->parse_errors++;
				return;
			}
			switch (ext[i]) {
			case BT_MONITOR_TS32:
				/* Type byte plus 32-bit timestamp */
				i += 1 + sizeof(uint32_t);
				break;
			case BT_MONITOR_ACL_TX_DROPS:
				st->acl_tx_drops += ext[i + 1];
				st->largest_acl_tx_drop = MAX(st->largest_acl_tx_drop, ext[i + 1]);
				/* Type byte plus 8-bit drop count */
				i += 2;
				break;
			case BT_MONITOR_COMMAND_DROPS:
			case BT_MONITOR_EVENT_DROPS:
			case BT_MONITOR_ACL_RX_DROPS:
			case BT_MONITOR_SCO_RX_DROPS:
			case BT_MONITOR_SCO_TX_DROPS:
			case BT_MONITOR_OTHER_DROPS:
				/* Type byte plus 8-bit drop count */
				i += 2;
				break;
			default:
				st->parse_errors++;
				return;
			}
		}

		st->opcode_mask |= BIT(opcode);
		if (opcode == TEST_OPCODE || opcode == BT_MONITOR_COMMAND_PKT ||
		    opcode == BT_MONITOR_EVENT_PKT) {
			uint32_t seq;

			if (payload_len != TEST_PAYLOAD_LEN) {
				st->seq_errors++;
			} else {
				seq = sys_get_le32(payload);
				/* Sequence numbers must be strictly
				 * increasing; gaps are drops, not errors.
				 */
				if (have_seq && seq < expect_seq) {
					st->seq_errors++;
				}
				for (size_t j = sizeof(seq); j < TEST_PAYLOAD_LEN; j++) {
					if (payload[j] != PATTERN_BYTE(j)) {
						st->seq_errors++;
						break;
					}
				}
				st->records++;
				st->last_seq = seq;
				expect_seq = seq + 1;
				have_seq = true;
			}
		}

		pos += sizeof(data_len) + data_len;
	}
	if (pos != stream_len) {
		st->parse_errors++;
	}
}

static int monitor_test_before(void **state)
{
	(void)state;
	ring_buf_init(&monitor_buf, sizeof(monitor_storage), monitor_storage);
	reject_output = false;
	stream_len = 0;
	tx_seq = 0;
	assert_int_equal(bt_monitor_init(buffer_sink, &monitor_buf), 0);
	return 0;
}

static int monitor_test_after(void **state)
{
	(void)state;
	int result = 0;

	if (output_file) {
		result = fclose(output_file);
		output_file = NULL;
	}
	/* No producer runs during the fixture's sink replacement. */
	assert_int_equal(bt_monitor_init(buffer_sink, &monitor_buf), 0);
	return result;
}

static void bt_monitor__test_single_record(void **cm_state)
{
	(void)cm_state;
	struct stream_stats st;
	uint64_t before = bt_uptime_get_us() / 100;

	send_record();
	capture();
	parse_stream(&st);
	uint64_t after = bt_uptime_get_us() / 100;

	assert_int_equal(st.parse_errors, 0);
	assert_int_equal(st.seq_errors, 0);
	assert_int_equal(st.records, 1);
	assert_int_equal(st.last_seq, 0);
	assert_int_equal(st.acl_tx_drops, 0);
	assert_int_equal(stream_len, BT_MONITOR_BASE_HDR_LEN + sizeof(struct bt_monitor_ts32) +
			 TEST_PAYLOAD_LEN);
	assert_true((uint32_t)(sys_get_le32(stream + BT_MONITOR_BASE_HDR_LEN + 1) -
			      (uint32_t)before) <= after - before);
}

static void bt_monitor__test_stream_across_wrap(void **cm_state)
{
	(void)cm_state;
	struct stream_stats st;

	/* Each record is ~40 bytes against a 128-byte monitor buffer, so
	 * this crosses the ring buffer wrap point many times. The capture
	 * is drained after every record, so nothing may be dropped.
	 */
	for (int i = 0; i < 64; i++) {
		send_record();
		capture();
	}
	parse_stream(&st);

	assert_int_equal(st.parse_errors, 0);
	assert_int_equal(st.seq_errors, 0);
	assert_int_equal(st.records, 64);
	assert_int_equal(st.last_seq, 63);
	assert_int_equal(st.acl_tx_drops, 0);
}

static void bt_monitor__test_overflow_drops_whole_records(void **cm_state)
{
	(void)cm_state;
	struct stream_stats st;
	uint32_t sent;

	/* Pause the consumer so the bounded sink fills up. Rejected records
	 * must not publish a partial header or lose accumulated drop counts.
	 */
	for (int i = 0; i < 16; i++) {
		send_record();
	}

	/* Resume consumption in stages and publish the accumulated drops. */
	for (int i = 0; i < 16; i++) {
		capture();
		send_record();
	}
	capture();
	sent = tx_seq;
	parse_stream(&st);

	assert_int_equal(st.parse_errors, 0);
	assert_int_equal(st.seq_errors, 0);
	assert_true(st.acl_tx_drops > 0);
	assert_int_equal(st.records + st.acl_tx_drops, sent);
}

/** @brief Rejected records must preserve both their bytes and previous drop counts. */
static void bt_monitor__test_repeated_rejection_preserves_drops(void **state)
{
	(void)state;
	struct stream_stats st;

	send_record();
	uint32_t queued = ring_buf_size_get(&monitor_buf);

	reject_output = true;
	for (int i = 0; i < 7; i++) {
		send_record();
		assert_int_equal(ring_buf_size_get(&monitor_buf), queued);
	}
	reject_output = false;
	capture();
	send_record();
	capture();
	parse_stream(&st);

	assert_int_equal(st.parse_errors, 0);
	assert_int_equal(st.seq_errors, 0);
	assert_int_equal(st.records, 2);
	assert_int_equal(st.acl_tx_drops, 7);
	assert_int_equal(st.last_seq, 8);
	assert_int_equal(st.records + st.acl_tx_drops, tx_seq);

	/* A subsequent accepted record must not report the same drops again. */
	send_record();
	capture();
	parse_stream(&st);
	assert_int_equal(st.parse_errors, 0);
	assert_int_equal(st.seq_errors, 0);
	assert_int_equal(st.records, 3);
	assert_int_equal(st.acl_tx_drops, 7);
}

/** @brief Report byte-sized drop fields across records without losing the surplus. */
static void bt_monitor__test_drop_count_over_255(void **state)
{
	(void)state;
	struct stream_stats st;

	reject_output = true;
	for (int i = 0; i < 300; i++) {
		send_record();
		assert_true(ring_buf_is_empty(&monitor_buf));
	}
	reject_output = false;

	for (uint32_t i = 0; i < 3; i++) {
		send_record();
		capture();
		parse_stream(&st);
		assert_int_equal(st.parse_errors, 0);
		assert_int_equal(st.seq_errors, 0);
		assert_int_equal(st.records, i + 1);
		assert_int_equal(st.acl_tx_drops, i == 0 ? 255 : 300);
		assert_int_equal(st.largest_acl_tx_drop, 255);
	}
	assert_int_equal(st.last_seq, 302);
	assert_int_equal(st.records + st.acl_tx_drops, tx_seq);
}

/** @brief Read back the real binary framing and ordered payloads from a file sink. */
static void bt_monitor__test_file_output(void **state)
{
	(void)state;
	static const uint16_t opcodes[] = {
		BT_MONITOR_COMMAND_PKT, BT_MONITOR_EVENT_PKT, BT_MONITOR_ACL_TX_PKT,
	};
	struct stream_stats st;
	size_t pos = 0;

	output_file = tmpfile();
	assert_non_null(output_file);
	assert_int_equal(bt_monitor_init(file_sink, output_file), 0);
	for (size_t i = 0; i < ARRAY_SIZE(opcodes); i++) {
		send_record_opcode(opcodes[i]);
	}
	assert_int_equal(fflush(output_file), 0);
	assert_int_equal(fseek(output_file, 0, SEEK_SET), 0);
	stream_len = fread(stream, 1, sizeof(stream), output_file);
	assert_int_equal(ferror(output_file), 0);
	assert_true(feof(output_file));
	assert_int_equal(stream_len, ARRAY_SIZE(opcodes) *
			 (BT_MONITOR_BASE_HDR_LEN + sizeof(struct bt_monitor_ts32) +
			  TEST_PAYLOAD_LEN));
	parse_stream(&st);
	assert_int_equal(st.parse_errors, 0);
	assert_int_equal(st.seq_errors, 0);
	assert_int_equal(st.records, ARRAY_SIZE(opcodes));
	assert_int_equal(st.last_seq, 2);
	assert_int_equal(st.acl_tx_drops, 0);
	assert_int_equal(st.opcode_mask, BIT(BT_MONITOR_COMMAND_PKT) |
			 BIT(BT_MONITOR_EVENT_PKT) | BIT(BT_MONITOR_ACL_TX_PKT));

	for (size_t i = 0; i < ARRAY_SIZE(opcodes); i++) {
		uint16_t opcode = sys_get_le16(stream + pos +
					     offsetof(struct bt_monitor_hdr, opcode));

		assert_int_equal(opcode, opcodes[i]);
		pos += sizeof(uint16_t) + sys_get_le16(stream + pos);
	}
	assert_int_equal(pos, stream_len);
}

#define MONITOR_CASE(name) \
	cmocka_unit_test_setup_teardown(bt_monitor__##name, monitor_test_before, monitor_test_after)

static const struct CMUnitTest monitor_cases[] = {
	MONITOR_CASE(test_single_record),
	MONITOR_CASE(test_stream_across_wrap),
	MONITOR_CASE(test_overflow_drops_whole_records),
	MONITOR_CASE(test_repeated_rejection_preserves_drops),
	MONITOR_CASE(test_drop_count_over_255),
	MONITOR_CASE(test_file_output),
};
BT_TEST_SUITE(bt_monitor, monitor_cases, bt_test_base_setup, NULL);
