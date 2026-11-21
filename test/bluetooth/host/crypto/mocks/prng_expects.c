#include "support/test_support.h"
/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "mocks/prng.h"
#include "mocks/prng_expects.h"

void expect_single_call_tc_psa_crypto_init(void)
{
	const char *func_name = "psa_crypto_init";

	assert_int_equal(psa_crypto_init_fake.call_count, 1);
}

void expect_single_call_psa_generate_random(uint8_t *out, size_t outlen)
{
	const char *func_name = "psa_generate_random";

	assert_int_equal(psa_generate_random_fake.call_count, 1);

	assert_ptr_equal(psa_generate_random_fake.arg0_val, out);
	assert_int_equal(psa_generate_random_fake.arg1_val, outlen);
}
