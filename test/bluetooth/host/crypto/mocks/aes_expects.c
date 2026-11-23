#include "support/test_support.h"
/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "mocks/aes.h"
#include "mocks/aes_expects.h"

void expect_single_call_psa_cipher_encrypt(uint8_t *out)
{
	const char *func_name = "psa_cipher_encrypt";

	assert_int_equal(psa_cipher_encrypt_fake.call_count, 1);

	assert_int_not_equal(psa_cipher_encrypt_fake.arg1_val, 0);
	assert_int_not_equal(psa_cipher_encrypt_fake.arg3_val, 0);
	assert_ptr_equal(psa_cipher_encrypt_fake.arg4_val, out);
	assert_int_not_equal(psa_cipher_encrypt_fake.arg5_val, 0);
}
