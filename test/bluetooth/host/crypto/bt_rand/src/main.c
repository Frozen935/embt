#include "support/test_support.h"
void fff_reset_rule_before(const struct bt_test_case_info *, void *);

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mocks/hci_core.h"
#include "mocks/hci_core_expects.h"
#include "mocks/prng.h"
#include "mocks/prng_expects.h"

#include <embt/bluetooth/crypto.h>
#include <fff.h>


#include <host/crypto.h>

DEFINE_FFF_GLOBALS;

void fff_reset_rule_before(const struct bt_test_case_info *test, void *fixture)
{
	HCI_CORE_FFF_FAKES_LIST(RESET_FAKE);
	PRNG_FFF_FAKES_LIST(RESET_FAKE);
}





/*
 *  Test bt_rand() succeeds while 'CONFIG_BT_HOST_CRYPTO_PRNG' isn't enabled.
 *
 *  Constraints:
 *   - 'CONFIG_BT_HOST_CRYPTO_PRNG' isn't enabled
 *   - bt_hci_le_rand() succeeds and returns 0 (success)
 *
 *  Expected behaviour:
 *   - bt_rand() returns 0 (success)
 */
static void bt_rand__test_bt_rand_succeeds_host_crypto_prng_disabled(void **cm_state)
{
	(void)cm_state;
	int err;
	uint8_t buf[16];
	size_t buf_len = 16;
	uint8_t expected_args_history[] = {16};

	#if defined(CONFIG_BT_HOST_CRYPTO_PRNG)
	skip();
#endif


	bt_hci_le_rand_fake.return_val = 0;

	err = bt_rand(buf, buf_len);

	expect_call_count_bt_hci_le_rand(1, expected_args_history);

	assert_int_equal(err, 0);
}

/*
 *  Test bt_rand() succeeds when psa_generate_random() succeeds on the first call while
 * 'CONFIG_BT_HOST_CRYPTO_PRNG' is enabled.
 *
 *  Constraints:
 *   - 'CONFIG_BT_HOST_CRYPTO_PRNG' is enabled
 *   - psa_generate_random() succeeds and returns 'PSA_SUCCESS' on the first call.
 *
 *  Expected behaviour:
 *   - bt_rand() returns 0 (success)
 */
static void bt_rand__test_psa_generate_random_succeeds_on_first_call(void **cm_state)
{
	(void)cm_state;
	int err;
	uint8_t buf[16];
	size_t buf_len = 16;

	#if !defined(CONFIG_BT_HOST_CRYPTO_PRNG)
	skip();
#endif


	psa_generate_random_fake.return_val = PSA_SUCCESS;

	err = bt_rand(buf, buf_len);

	expect_single_call_psa_generate_random(buf, buf_len);

	assert_int_equal(err, 0);
}


static int bt_test_bt_rand_before(void **state)
{
	(void)state;
	fff_reset_rule_before(NULL, *state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_rand[] = {
	cmocka_unit_test_setup_teardown(bt_rand__test_bt_rand_succeeds_host_crypto_prng_disabled, bt_test_bt_rand_before, NULL),
	cmocka_unit_test_setup_teardown(bt_rand__test_psa_generate_random_succeeds_on_first_call, bt_test_bt_rand_before, NULL),
};
BT_TEST_SUITE(bt_rand, test_cases_bt_rand, bt_test_base_setup, NULL);
