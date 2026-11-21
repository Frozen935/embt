#include "support/test_support.h"
/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <embt/net_buf.h>

#include "host/classic/at.h"
static struct at_client at;

NET_BUF_POOL_DEFINE(at_pool, 1, 140, 0, NULL);

static const char example_data[] = "\r\n+ABCD:999\r\n";

int at_handle(struct at_client *hf_at)
{
	uint32_t val;

	assert_int_equal(at_get_number(hf_at, &val), 0);

	assert_int_equal(val, 999);

	return 0;
}

int at_resp(struct at_client *hf_at, struct net_buf *buf)
{
	int err;

	err = at_parse_cmd_input(hf_at, buf, "ABCD", at_handle,
				 AT_CMD_TYPE_NORMAL);
	assert_int_equal(err, 0);

	return 0;
}



static void at_tests__test_at(void **cm_state)
{
	(void)cm_state;
	struct net_buf *buf;
	int len;

	buf = net_buf_alloc(&at_pool, BT_FOREVER);
	assert_non_null(buf);

	at_register(&at, at_resp, NULL);
	len = strlen(example_data);

	assert_true(net_buf_tailroom(buf) >= len);
	net_buf_add_mem(buf, example_data, len);

	assert_int_equal(at_parse_input(&at, buf), 0);
}

static void storage_init_0abb49a14e(void)
{
	net_buf_pool_init(&at_pool);
}

static void migrated_pools_init(void)
{
	net_buf_pool_init(&at_pool);
}

static int bt_test_at_tests_setup(void **state)
{
	int environment_result = bt_test_host_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	migrated_pools_init();
	storage_init_0abb49a14e();
	*state = NULL;
	return 0;
}



static const struct CMUnitTest test_cases_at_tests[] = {
	cmocka_unit_test(at_tests__test_at),
};
BT_TEST_SUITE(at_tests, test_cases_at_tests, bt_test_at_tests_setup, NULL);
