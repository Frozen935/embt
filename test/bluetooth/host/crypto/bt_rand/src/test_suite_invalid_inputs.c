#include "support/test_support.h"
void fff_reset_rule_before(const struct bt_test_case_info *, void *);

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "host_mocks/assert.h"
#include "mocks/hci_core.h"
#include "mocks/hci_core_expects.h"
#include "mocks/prng.h"
#include "mocks/prng_expects.h"

#include <embt/bluetooth/crypto.h>


#include <host/crypto.h>



/*
 *  Test passing NULL reference destination buffer argument
 *
 *  Constraints:
 *   - NULL reference is used as an argument for the destination buffer
 *
 *  Expected behaviour:
 *   - EmBT returns -EINVAL for invalid input without accessing it
 */
static void bt_rand_invalid_cases__test_null_dst_buf_reference(void **cm_state)
{
	(void)cm_state;
	assert_int_equal(bt_rand(NULL, 1), -EINVAL);
}

/*
 *  Test passing a valid destination buffer reference with size 0
 *
 *  Constraints:
 *   - A valid reference is used as an argument for the destination buffer
 *   - Destination buffer size is passed as 0
 *
 *  Expected behaviour:
 *   - EmBT returns -EINVAL for invalid input without accessing it
 */
static void bt_rand_invalid_cases__test_zero_dst_buf_size_reference(void **cm_state)
{
	(void)cm_state;
	uint8_t buf[16];

	assert_int_equal(bt_rand(buf, 0), -EINVAL);
}

/*
 *  Test bt_rand() fails when bt_hci_le_rand() fails while 'CONFIG_BT_HOST_CRYPTO_PRNG'
 *  isn't enabled.
 *
 *  Constraints:
 *   - 'CONFIG_BT_HOST_CRYPTO_PRNG' isn't enabled
 *   - bt_hci_le_rand() fails and returns a negative error code.
 *
 *  Expected behaviour:
 *   - bt_rand() returns a negative error code (failure)
 */
static void bt_rand_invalid_cases__test_bt_hci_le_rand_fails(void **cm_state)
{
	(void)cm_state;
	int err;
	uint8_t buf[16];
	size_t buf_len = 16;
	uint8_t expected_args_history[] = {16};

	#if defined(CONFIG_BT_HOST_CRYPTO_PRNG)
	skip();
#endif


	bt_hci_le_rand_fake.return_val = -1;

	err = bt_rand(buf, buf_len);

	expect_call_count_bt_hci_le_rand(1, expected_args_history);

	assert_true(err < 0);
}

/*
 *  Test bt_rand() fails when psa_generate_random() fails on the first call while
 * 'CONFIG_BT_HOST_CRYPTO_PRNG' is enabled.
 *
 *  Constraints:
 *   - 'CONFIG_BT_HOST_CRYPTO_PRNG' is enabled
 *   - psa_generate_random() fails and returns '-EIO' on the first call.
 *
 *  Expected behaviour:
 *   - bt_rand() returns a negative error code '-EIO' (failure)
 */
static void bt_rand_invalid_cases__test_tc_hmac_prng_generate_fails_on_first_call(void **cm_state)
{
	(void)cm_state;
	int err;
	uint8_t buf[16];
	size_t buf_len = 16;

	#if !defined(CONFIG_BT_HOST_CRYPTO_PRNG)
	skip();
#endif


	psa_generate_random_fake.return_val = -EIO;

	err = bt_rand(buf, buf_len);

	expect_single_call_psa_generate_random(buf, buf_len);

	assert_true(err == -EIO);
}


static int bt_test_bt_rand_invalid_cases_before(void **state)
{
	(void)state;
	fff_reset_rule_before(NULL, *state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_rand_invalid_cases[] = {
	cmocka_unit_test_setup_teardown(bt_rand_invalid_cases__test_null_dst_buf_reference, bt_test_bt_rand_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_rand_invalid_cases__test_zero_dst_buf_size_reference, bt_test_bt_rand_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_rand_invalid_cases__test_bt_hci_le_rand_fails, bt_test_bt_rand_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_rand_invalid_cases__test_tc_hmac_prng_generate_fails_on_first_call, bt_test_bt_rand_invalid_cases_before, NULL),
};
BT_TEST_SUITE(bt_rand_invalid_cases, test_cases_bt_rand_invalid_cases, bt_test_base_setup, NULL);
