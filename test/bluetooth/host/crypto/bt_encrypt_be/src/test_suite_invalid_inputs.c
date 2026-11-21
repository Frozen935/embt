#include "support/test_support.h"
void fff_reset_rule_before(const struct bt_test_case_info *, void *);

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "host_mocks/assert.h"
#include "mocks/aes.h"

#include <embt/bluetooth/crypto.h>


#include <host/crypto.h>



/*
 *  Test passing NULL reference for the key argument
 *
 *  Constraints:
 *   - NULL reference is used for the key argument
 *   - Valid references are used for the other arguments
 *
 *  Expected behaviour:
 *   - EmBT returns -EINVAL for invalid input without accessing it
 */
static void bt_encrypt_be_invalid_cases__test_null_key_reference(void **cm_state)
{
	(void)cm_state;
	const uint8_t plaintext[16] = {0};
	uint8_t enc_data[16] = {0};

	assert_int_equal(bt_encrypt_le(NULL, plaintext, enc_data), -EINVAL);
}

/*
 *  Test passing NULL reference for the plain text argument
 *
 *  Constraints:
 *   - NULL reference is used for the plain text argument
 *   - Valid references are used for the other arguments
 *
 *  Expected behaviour:
 *   - EmBT returns -EINVAL for invalid input without accessing it
 */
static void bt_encrypt_be_invalid_cases__test_null_plaintext_reference(void **cm_state)
{
	(void)cm_state;
	const uint8_t key[16] = {0};
	uint8_t enc_data[16] = {0};

	assert_int_equal(bt_encrypt_le(key, NULL, enc_data), -EINVAL);
}

/*
 *  Test passing NULL reference for the encrypted data destination buffer argument
 *
 *  Constraints:
 *   - NULL reference is used for the encrypted data destination buffer argument
 *   - Valid references are used for the other arguments
 *
 *  Expected behaviour:
 *   - EmBT returns -EINVAL for invalid input without accessing it
 */
static void bt_encrypt_be_invalid_cases__test_null_enc_data_reference(void **cm_state)
{
	(void)cm_state;
	const uint8_t key[16] = {0};
	const uint8_t plaintext[16] = {0};

	assert_int_equal(bt_encrypt_le(key, plaintext, NULL), -EINVAL);
}

/*
 *  Test bt_encrypt_le() fails when tc_aes128_set_encrypt_key() fails
 *
 *  Constraints:
 *   - psa_import_key() fails and returns 'PSA_ERROR_GENERIC_ERROR'.
 *
 *  Expected behaviour:
 *   - bt_encrypt_le() returns a negative error code '-EINVAL' (failure)
 */
static void bt_encrypt_be_invalid_cases__test_psa_import_key_fails(void **cm_state)
{
	(void)cm_state;
	int err;
	const uint8_t key[16] = {0};
	const uint8_t plaintext[16] = {0};
	uint8_t enc_data[16] = {0};

	psa_import_key_fake.return_val = PSA_ERROR_GENERIC_ERROR;

	err = bt_encrypt_le(key, plaintext, enc_data);

	assert_true(err == -EINVAL);
}

/*
 *  Test bt_encrypt_le() fails when tc_aes_encrypt() fails
 *
 *  Constraints:
 *   - psa_import_key() succeeds and returns 'PSA_SUCCESS'.
 *   - psa_cipher_encrypt() fails and returns 'PSA_ERROR_GENERIC_ERROR'.
 *
 *  Expected behaviour:
 *   - bt_encrypt_le() returns a negative error code '-EINVAL' (failure)
 */
static void bt_encrypt_be_invalid_cases__test_psa_cipher_encrypt_fails(void **cm_state)
{
	(void)cm_state;
	int err;
	const uint8_t key[16] = {0};
	const uint8_t plaintext[16] = {0};
	uint8_t enc_data[16] = {0};

	psa_import_key_fake.return_val = PSA_SUCCESS;
	psa_cipher_encrypt_fake.return_val = -EINVAL;

	err = bt_encrypt_le(key, plaintext, enc_data);

	assert_true(err == -EIO);
}


static int bt_test_bt_encrypt_be_invalid_cases_before(void **state)
{
	(void)state;
	fff_reset_rule_before(NULL, *state);
	return 0;
}


static const struct CMUnitTest test_cases_bt_encrypt_be_invalid_cases[] = {
	cmocka_unit_test_setup_teardown(bt_encrypt_be_invalid_cases__test_null_key_reference, bt_test_bt_encrypt_be_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_encrypt_be_invalid_cases__test_null_plaintext_reference, bt_test_bt_encrypt_be_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_encrypt_be_invalid_cases__test_null_enc_data_reference, bt_test_bt_encrypt_be_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_encrypt_be_invalid_cases__test_psa_import_key_fails, bt_test_bt_encrypt_be_invalid_cases_before, NULL),
	cmocka_unit_test_setup_teardown(bt_encrypt_be_invalid_cases__test_psa_cipher_encrypt_fails, bt_test_bt_encrypt_be_invalid_cases_before, NULL),
};
BT_TEST_SUITE(bt_encrypt_be_invalid_cases, test_cases_bt_encrypt_be_invalid_cases, bt_test_base_setup, NULL);
