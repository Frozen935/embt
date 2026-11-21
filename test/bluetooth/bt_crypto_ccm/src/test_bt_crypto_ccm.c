#include "support/test_support.h"
/* Copyright (c) 2023 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <embt/base/log.h>
#include <embt/bluetooth/crypto.h>

#include "common/bt_str.h"

#include "test_vectors.h"

LOG_MODULE_REGISTER(test_bt_crypto_ccm, LOG_LEVEL_INF);



static void bt_crypto_ccm__test_result_rfc_test_vectors(void **cm_state)
{
	(void)cm_state;
	for (int i = 0; i < NUMBER_OF_TEST; i++) {
		LOG_DBG("=============== Packet Vector #%d ==================", i + 1);
		int err;
		struct test_data *p = input_packets[i];

		p->expected_output_len = p->input_len + p->mic_len;

		const uint8_t *aad = p->input;

		uint8_t *encrypted_data =
			(uint8_t *)malloc(p->expected_output_len * sizeof(uint8_t));

		memcpy(encrypted_data, p->input, p->input_len);

		err = bt_ccm_encrypt(p->key, p->nonce, &p->input[p->aad_len],
				     p->input_len - p->aad_len, aad, p->aad_len,
				     &encrypted_data[p->aad_len], p->mic_len);
		assert_true(err == 0);

		LOG_DBG("encrypted data %s (len: %d)",
			bt_hex(encrypted_data, p->expected_output_len), p->expected_output_len);
		LOG_DBG("expected data  %s (len: %d)",
			bt_hex(p->expected_output, p->expected_output_len), p->expected_output_len);

		assert_memory_equal(encrypted_data, p->expected_output, p->expected_output_len);

		uint8_t *decrypted_data = (uint8_t *)malloc(p->input_len * sizeof(uint8_t));

		memcpy(decrypted_data, encrypted_data, p->aad_len);

		err = bt_ccm_decrypt(p->key, p->nonce, &encrypted_data[p->aad_len],
				     p->input_len - p->aad_len, aad, p->aad_len,
				     &decrypted_data[p->aad_len], p->mic_len);
		assert_true(err == 0);

		LOG_DBG("decrypted data %s (len: %d)", bt_hex(decrypted_data, p->input_len),
			p->input_len);
		LOG_DBG("expected data %s (len: %d)", bt_hex(p->input, p->input_len), p->input_len);

		assert_memory_equal(decrypted_data, p->input, p->input_len);

		free(encrypted_data);
		free(decrypted_data);
	}
}

static void bt_crypto_ccm__test_decrypt_invalid_mic(void **cm_state)
{
	(void)cm_state;
	struct test_data *p = input_packets[0];
	const size_t enc_len = p->input_len + p->mic_len;
	const size_t payload_len = p->input_len - p->aad_len;
	uint8_t encrypted[TEST_VECTOR_MAX_ENC_LEN];
	uint8_t plaintext[TEST_VECTOR_MAX_INPUT_LEN];
	const uint8_t *aad = p->input;
	int err;

	memcpy(encrypted, p->expected_output, enc_len);
	memcpy(plaintext, p->input, p->aad_len);

	encrypted[enc_len - 1] ^= 0x01;

	err = bt_ccm_decrypt(p->key, p->nonce, &encrypted[p->aad_len], payload_len, aad,
			     p->aad_len, &plaintext[p->aad_len], p->mic_len);
	assert_int_equal(err, -EBADMSG);
}





static const struct CMUnitTest test_cases_bt_crypto_ccm[] = {
	cmocka_unit_test(bt_crypto_ccm__test_result_rfc_test_vectors),
	cmocka_unit_test(bt_crypto_ccm__test_decrypt_invalid_mic),
};
BT_TEST_SUITE(bt_crypto_ccm, test_cases_bt_crypto_ccm, bt_test_host_setup, NULL);
