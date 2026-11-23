#include "support/test_support.h"
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mocks/conn.h"
#include "mocks/hci_core.h"
#include "mocks/net_buf.h"


#include <embt/bluetooth/cs.h>
#include <fff.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(bool, bt_le_cs_step_data_parse_func, struct bt_le_cs_subevent_step *, void *);

void fff_reset_rule_before(const struct bt_test_case_info *test, void *fixture)
{
	RESET_FAKE(bt_le_cs_step_data_parse_func);
	CONN_FFF_FAKES_LIST(RESET_FAKE);
}





/*
 *  Test empty data buffer
 *
 *  Constraints:
 *   - buffer len set to 0
 *
 *  Expected behaviour:
 *   - Callback function is not called
 */
static void bt_le_cs_step_data_parse__test_parsing_empty_buf(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple *buf = NET_BUF_SIMPLE(0);

	bt_le_cs_step_data_parse(buf, bt_le_cs_step_data_parse_func, NULL);

	assert_int_equal(bt_le_cs_step_data_parse_func_fake.call_count, 0);
}

/*
 *  Test malformed step data
 *
 *  Constraints:
 *   - step data with a step length going out of bounds
 *
 *  Expected behaviour:
 *   - Callback function is called once
 */
static void bt_le_cs_step_data_parse__test_parsing_invalid_length(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		0x00, 0x01, 0x01, 0x00,       /* mode 0 */
		0x03, 0x20, 0x03, 0x00, 0x11, /* mode 3 step with bad length */
	};

	bt_le_cs_step_data_parse_func_fake.return_val = true;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_le_cs_step_data_parse(&buf, bt_le_cs_step_data_parse_func, NULL);

	assert_int_equal(1, bt_le_cs_step_data_parse_func_fake.call_count);
}

/*
 *  Test parsing stopped
 *
 *  Constraints:
 *   - Data contains valid step data
 *   - Callback function returns false to stop parsing
 *
 *  Expected behaviour:
 *   - Once parsing is stopped, the callback is not called anymore
 */
static void bt_le_cs_step_data_parse__test_parsing_stopped(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		0x00, 0x05, 0x01, 0x00,       /* mode 0 */
		0x01, 0x10, 0x02, 0x00, 0x11, /* mode 1 */
		0x02, 0x11, 0x02, 0x00, 0x11, /* mode 2 */
	};

	bt_le_cs_step_data_parse_func_fake.return_val = false;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_le_cs_step_data_parse(&buf, bt_le_cs_step_data_parse_func, NULL);

	assert_int_equal(1, bt_le_cs_step_data_parse_func_fake.call_count);
}

struct custom_user_data {
	const uint8_t *data;
	size_t len;
};

static bool bt_le_cs_step_data_parse_func_custom_fake(struct bt_le_cs_subevent_step *step,
						      void *user_data)
{
	struct custom_user_data *ud = user_data;

	/* mode check */
	assert_true(ud->len-- > 0);
	assert_int_equal(step->mode, *ud->data);
	ud->data++;

	/* channel check */
	assert_true(ud->len-- > 0);
	assert_int_equal(step->channel, *ud->data);
	ud->data++;

	/* step data length check */
	assert_true(ud->len-- > 0);
	assert_int_equal(step->data_len, *ud->data);
	ud->data++;

	/* value check */
	assert_true(ud->len >= step->data_len);
	assert_memory_equal(step->data, ud->data, step->data_len);
	ud->data += step->data_len;
	ud->len -= step->data_len;

	return true;
}

/*
 *  Test parsing successfully
 *
 *  Constraints:
 *   - Data contains valid step data
 *   - Callback function returns false to stop parsing
 *
 *  Expected behaviour:
 *   - Data passed to the callback match the expected data
 */
static void bt_le_cs_step_data_parse__test_parsing_success(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		0x00, 0x05, 0x01, 0x00,       /* mode 0 */
		0x03, 0x11, 0x01, 0x11,       /* mode 3 */
		0x02, 0x12, 0x02, 0x00, 0x11, /* mode 2 */
		0x03, 0x13, 0x01, 0x11,       /* mode 3 */
		0x02, 0x14, 0x02, 0x00, 0x11, /* mode 2 */
	};

	struct custom_user_data user_data = {
		.data = data,
		.len = ARRAY_SIZE(data),
	};

	bt_le_cs_step_data_parse_func_fake.custom_fake = bt_le_cs_step_data_parse_func_custom_fake;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_le_cs_step_data_parse(&buf, bt_le_cs_step_data_parse_func, &user_data);

	assert_int_equal(5, bt_le_cs_step_data_parse_func_fake.call_count);
}


static int bt_test_bt_le_cs_step_data_parse_before(void **state)
{
	(void)state;
	fff_reset_rule_before(NULL, *state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_le_cs_step_data_parse[] = {
	cmocka_unit_test_setup_teardown(bt_le_cs_step_data_parse__test_parsing_empty_buf, bt_test_bt_le_cs_step_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_le_cs_step_data_parse__test_parsing_invalid_length, bt_test_bt_le_cs_step_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_le_cs_step_data_parse__test_parsing_stopped, bt_test_bt_le_cs_step_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_le_cs_step_data_parse__test_parsing_success, bt_test_bt_le_cs_step_data_parse_before, NULL),
};
BT_TEST_SUITE(bt_le_cs_step_data_parse, test_cases_bt_le_cs_step_data_parse, bt_test_base_setup, NULL);
