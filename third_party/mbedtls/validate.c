/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#include "requirements.h"
#include <psa/crypto.h>
#include <stdio.h>
#include <string.h>
#if EMBT_CHECK_HELPERS
#include <mbedtls/constant_time.h>
#endif

#define CHECK(call) do { \
	psa_status_t status = (call); \
	if (status != PSA_SUCCESS) { \
		fprintf(stderr, "PSA capability failed: %s (%ld)\n", #call, (long)status); \
		return 1; \
	} \
} while (0)

#if EMBT_CHECK_ECB || EMBT_CHECK_CMAC || EMBT_CHECK_CCM
static const unsigned char aes_key[16] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};
static const unsigned char message[16] = {
	0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
	0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
};
#endif

static int validate_crypto(void)
{
	unsigned char output[80];
	size_t length;

	CHECK(psa_crypto_init());
	CHECK(psa_generate_random(output, sizeof(output)));
#if EMBT_CHECK_ECB || EMBT_CHECK_CMAC || EMBT_CHECK_CCM
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t key;

	psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&attributes, 128);
#if EMBT_CHECK_ECB
	static const unsigned char expected_ecb[16] = {
		0x3a, 0xd7, 0x7b, 0xb4, 0x0d, 0x7a, 0x36, 0x60,
		0xa8, 0x9e, 0xca, 0xf3, 0x24, 0x66, 0xef, 0x97
	};

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING);
	CHECK(psa_import_key(&attributes, aes_key, sizeof(aes_key), &key));
	CHECK(psa_cipher_encrypt(key, PSA_ALG_ECB_NO_PADDING, message, sizeof(message),
				output, sizeof(output), &length));
	CHECK(psa_destroy_key(key));
	if (length != 16 || memcmp(output, expected_ecb, 16)) {
		fprintf(stderr, "PSA AES-ECB known vector mismatch\n");
		return 1;
	}
#endif
#if EMBT_CHECK_CMAC
	static const unsigned char expected_cmac[16] = {
		0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
		0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c
	};

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&attributes, PSA_ALG_CMAC);
	CHECK(psa_import_key(&attributes, aes_key, sizeof(aes_key), &key));
	CHECK(psa_mac_compute(key, PSA_ALG_CMAC, message, sizeof(message),
			     output, sizeof(output), &length));
	CHECK(psa_destroy_key(key));
	if (length != 16 || memcmp(output, expected_cmac, 16)) {
		fprintf(stderr, "PSA AES-CMAC known vector mismatch\n");
		return 1;
	}
#endif
#if EMBT_CHECK_CCM
	unsigned char plaintext[16];
	unsigned char nonce[13] = {0};
	size_t plaintext_length;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&attributes, PSA_ALG_CCM);
	CHECK(psa_import_key(&attributes, aes_key, sizeof(aes_key), &key));
	CHECK(psa_aead_encrypt(key, PSA_ALG_CCM, nonce, sizeof(nonce), NULL, 0,
			      message, sizeof(message), output, sizeof(output), &length));
	CHECK(psa_aead_decrypt(key, PSA_ALG_CCM, nonce, sizeof(nonce), NULL, 0,
			      output, length, plaintext, sizeof(plaintext), &plaintext_length));
	CHECK(psa_destroy_key(key));
	if (plaintext_length != sizeof(message) || memcmp(message, plaintext, sizeof(message))) {
		fprintf(stderr, "PSA CCM round-trip mismatch\n");
		return 1;
	}
#endif
	psa_reset_key_attributes(&attributes);
#endif
#if EMBT_CHECK_ECDH
	psa_key_attributes_t ecc_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t first, second, imported;
	unsigned char first_public[65], second_public[65], private_key[32];
	unsigned char first_secret[32], second_secret[32];
	size_t first_size, second_size, private_size, first_secret_size, second_secret_size;

	psa_set_key_type(&ecc_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&ecc_attributes, 256);
	psa_set_key_usage_flags(&ecc_attributes, PSA_KEY_USAGE_DERIVE | PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&ecc_attributes, PSA_ALG_ECDH);
	CHECK(psa_generate_key(&ecc_attributes, &first));
	CHECK(psa_generate_key(&ecc_attributes, &second));
	CHECK(psa_export_public_key(first, first_public, sizeof(first_public), &first_size));
	CHECK(psa_export_public_key(second, second_public, sizeof(second_public), &second_size));
	CHECK(psa_export_key(first, private_key, sizeof(private_key), &private_size));
	CHECK(psa_import_key(&ecc_attributes, private_key, private_size, &imported));
	CHECK(psa_raw_key_agreement(PSA_ALG_ECDH, imported, second_public, second_size,
				   first_secret, sizeof(first_secret), &first_secret_size));
	CHECK(psa_raw_key_agreement(PSA_ALG_ECDH, second, first_public, first_size,
				   second_secret, sizeof(second_secret), &second_secret_size));
	CHECK(psa_destroy_key(first));
	CHECK(psa_destroy_key(second));
	CHECK(psa_destroy_key(imported));
	psa_set_key_type(&ecc_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_usage_flags(&ecc_attributes, PSA_KEY_USAGE_DERIVE);
	CHECK(psa_import_key(&ecc_attributes, first_public, first_size, &imported));
	CHECK(psa_destroy_key(imported));
	psa_reset_key_attributes(&ecc_attributes);
	if (first_secret_size != second_secret_size ||
	    memcmp(first_secret, second_secret, first_secret_size)) {
		fprintf(stderr, "PSA P-256 ECDH round-trip mismatch\n");
		return 1;
	}
#endif
#if EMBT_CHECK_HMAC
	psa_key_attributes_t hmac_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_id_t hmac_key;
	static const unsigned char hmac_material[20] = {
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
	};
	static const unsigned char expected_hmac[32] = {
		0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
		0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
		0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
		0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
	};

	psa_set_key_type(&hmac_attributes, PSA_KEY_TYPE_HMAC);
	psa_set_key_bits(&hmac_attributes, sizeof(hmac_material) * 8);
	psa_set_key_usage_flags(&hmac_attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
	psa_set_key_algorithm(&hmac_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	CHECK(psa_import_key(&hmac_attributes, hmac_material, sizeof(hmac_material), &hmac_key));
	CHECK(psa_mac_compute(hmac_key, PSA_ALG_HMAC(PSA_ALG_SHA_256),
			     (const unsigned char *)"Hi There", 8, output, sizeof(output), &length));
	CHECK(psa_destroy_key(hmac_key));
	psa_reset_key_attributes(&hmac_attributes);
	if (length != sizeof(expected_hmac) || memcmp(output, expected_hmac, sizeof(expected_hmac))) {
		fprintf(stderr, "PSA HMAC-SHA256 known vector mismatch\n");
		return 1;
	}
#endif
#if EMBT_CHECK_SHA1
	CHECK(psa_hash_compute(PSA_ALG_SHA_1, NULL, 0, output, sizeof(output), &length));
#endif
#if EMBT_CHECK_SHA256
	CHECK(psa_hash_compute(PSA_ALG_SHA_256, NULL, 0, output, sizeof(output), &length));
#endif
#if EMBT_CHECK_MD5
	CHECK(psa_hash_compute(PSA_ALG_MD5, NULL, 0, output, sizeof(output), &length));
#endif
#if EMBT_CHECK_HELPERS
	if (mbedtls_ct_memcmp("same", "same", 4) || !mbedtls_ct_memcmp("same", "diff", 4)) {
		fprintf(stderr, "Mbed TLS constant-time helper failed\n");
		return 1;
	}
#endif
	(void)length;
	return 0;
}

int main(void)
{
	int result = validate_crypto();

	if (!result) {
		puts("PSA capability check passed for the configured algorithms and RNG");
	}
	return result;
}
