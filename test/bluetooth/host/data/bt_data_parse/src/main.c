#include "support/test_support.h"
/*
 * Copyright (c) 2023 Codecoup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fff.h>

#include <embt/bluetooth/bluetooth.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(bool, bt_data_parse_func, struct bt_data *, void *);

void fff_reset_rule_before(const struct bt_test_case_info *test, void *fixture)
{
	RESET_FAKE(bt_data_parse_func);
}





/*
 *  Test empty data buffer
 *
 *  Constraints:
 *   - data.len set to 0
 *
 *  Expected behaviour:
 *   - Callback function is not called
 */
static void bt_data_parse__test_parsing_empty_buf(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple *buf = NET_BUF_SIMPLE(0);

	bt_data_parse(buf, bt_data_parse_func, NULL);

	assert_int_equal(bt_data_parse_func_fake.call_count, 0);
}

/*
 *  Test AD Structure invalid length
 *
 *  Constraints:
 *   - AD Structure N length > number of bytes after
 *
 *  Expected behaviour:
 *   - Callback function is called N - 1 times
 */
static void bt_data_parse__test_parsing_invalid_length(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		/* Significant part */
		0x02, 0x01, 0x00,                       /* AD Structure 1 */
		0x03, 0x02, 0x01, 0x00,                 /* AD Structure 2 */
		/* Invalid length 0xff */
		0xff, 0x03, 0x02, 0x01,                 /* AD Structure N */
		0x05, 0x04, 0x03, 0x02, 0x01, 0x00,     /* AD Structure N + 1 */
	};

	bt_data_parse_func_fake.return_val = true;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_data_parse(&buf, bt_data_parse_func, NULL);

	assert_int_equal(2, bt_data_parse_func_fake.call_count);
}

/*
 *  Test early termination of the significant part
 *
 *  Constraints:
 *   - The significant part contains a sequence of N AD structures
 *   - The non-significant part extends the data with all-zero octets
 *
 *  Expected behaviour:
 *   - Callback function is called N times
 */
static void bt_data_parse__test_parsing_early_termination(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		/* Significant part */
		0x02, 0x01, 0x00,                       /* AD Structure 1 */
		0x03, 0x02, 0x01, 0x00,                 /* AD Structure 2 */
		0x04, 0x03, 0x02, 0x01, 0x00,           /* AD Structure 3 */
		/* Non-significant part */
		0x00, 0x00, 0x00, 0x00, 0x00
	};

	bt_data_parse_func_fake.return_val = true;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_data_parse(&buf, bt_data_parse_func, NULL);

	assert_int_equal(3, bt_data_parse_func_fake.call_count);
}

/*
 *  Test parsing stopped
 *
 *  Constraints:
 *   - Data contains valid AD Structures
 *   - Callback function returns false to stop parsing
 *
 *  Expected behaviour:
 *   - Once parsing is stopped, the callback is not called anymore
 */
static void bt_data_parse__test_parsing_stopped(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		/* Significant part */
		0x02, 0x01, 0x00,                       /* AD Structure 1 */
		0x03, 0x02, 0x01, 0x00,                 /* AD Structure 2 */
	};

	bt_data_parse_func_fake.return_val = false;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_data_parse(&buf, bt_data_parse_func, NULL);

	assert_int_equal(1, bt_data_parse_func_fake.call_count);
}

struct custom_fake_user_data {
	const uint8_t *data;
	size_t len;
};

static bool bt_data_parse_func_custom_fake(struct bt_data *data,
					   void *user_data)
{
	struct custom_fake_user_data *ud = user_data;

	/* length check */
	assert_true(ud->len-- > 0);
	assert_int_equal(data->data_len, *ud->data - 1);
	ud->data++;

	/* type check */
	assert_true(ud->len-- > 0);
	assert_int_equal(data->type, *ud->data);
	ud->data++;

	/* value check */
	assert_true(ud->len >= data->data_len);
	assert_memory_equal(data->data, ud->data, data->data_len);
	ud->data += data->data_len;
	ud->len -= data->data_len;

	return true;
}

/*
 *  Test parsing AD Data
 *
 *  Constraints:
 *   - Data contains valid AD Structures
 *   - Callback function returns false to stop parsing
 *
 *  Expected behaviour:
 *   - Data passed to the callback match the expected data
 */
static void bt_data_parse__test_parsing_success(void **cm_state)
{
	(void)cm_state;
	struct net_buf_simple buf;
	uint8_t data[] = {
		/* Significant part */
		0x02, 0x01, 0x00,                       /* AD Structure 1 */
		0x03, 0x02, 0x01, 0x00,                 /* AD Structure 2 */
	};
	struct custom_fake_user_data user_data = {
		.data = data,
		.len = ARRAY_SIZE(data),
	};

	bt_data_parse_func_fake.custom_fake = bt_data_parse_func_custom_fake;

	net_buf_simple_init_with_data(&buf, data, ARRAY_SIZE(data));

	bt_data_parse(&buf, bt_data_parse_func, &user_data);

	assert_int_equal(2, bt_data_parse_func_fake.call_count);
}


static int bt_test_bt_data_parse_before(void **state)
{
	(void)state;
	fff_reset_rule_before(NULL, *state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_data_parse[] = {
	cmocka_unit_test_setup_teardown(bt_data_parse__test_parsing_empty_buf, bt_test_bt_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_data_parse__test_parsing_invalid_length, bt_test_bt_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_data_parse__test_parsing_early_termination, bt_test_bt_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_data_parse__test_parsing_stopped, bt_test_bt_data_parse_before, NULL),
	cmocka_unit_test_setup_teardown(bt_data_parse__test_parsing_success, bt_test_bt_data_parse_before, NULL),
};
BT_TEST_SUITE(bt_data_parse, test_cases_bt_data_parse, bt_test_base_setup, NULL);
