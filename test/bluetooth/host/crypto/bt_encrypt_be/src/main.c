#include "support/test_support.h"
void fff_reset_rule_before(const struct bt_test_case_info *, void *);

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mocks/aes.h"
#include "mocks/aes_expects.h"

#include <embt/bluetooth/crypto.h>
#include <fff.h>


#include <host/crypto.h>

DEFINE_FFF_GLOBALS;

void fff_reset_rule_before(const struct bt_test_case_info *test, void *fixture)
{
	AES_FFF_FAKES_LIST(RESET_FAKE);
}





/*
 *  Test bt_encrypt_be() succeeds
 *
 *  Constraints:
 *   - psa_import_key() succeeds and returns 'PSA_SUCCESS'.
 *   - psa_cipher_encrypt() succeeds and returns 'PSA_SUCCESS'.
 *
 *  Expected behaviour:
 *   - bt_encrypt_be() returns 0 (success)
 */
static void bt_encrypt_be__test_bt_encrypt_be_succeeds(void **cm_state)
{
	(void)cm_state;
	int err;
	const uint8_t key[16] = {0};
	const uint8_t plaintext[16] = {0};
	uint8_t enc_data[16] = {0};

	psa_import_key_fake.return_val = PSA_SUCCESS;
	psa_cipher_encrypt_fake.return_val = PSA_SUCCESS;

	err = bt_encrypt_be(key, plaintext, enc_data);

	expect_single_call_psa_cipher_encrypt(enc_data);

	assert_int_equal(err, 0);
}


static int bt_test_bt_encrypt_be_before(void **state)
{
	(void)state;
	fff_reset_rule_before(NULL, *state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_encrypt_be[] = {
	cmocka_unit_test_setup_teardown(bt_encrypt_be__test_bt_encrypt_be_succeeds, bt_test_bt_encrypt_be_before, NULL),
};
BT_TEST_SUITE(bt_encrypt_be, test_cases_bt_encrypt_be, bt_test_base_setup, NULL);
