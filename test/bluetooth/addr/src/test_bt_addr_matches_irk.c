#include "support/test_support.h"
/*
 * Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>

#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/crypto.h>
#include <embt/base/util.h>
/* Sample data for the random address hash function ah, from the Bluetooth Core
 * Specification, Vol 3, Part H, Appendix D.7:
 *
 *   IRK   = ec0234a3 57c8ad05 341010a6 0a397d9b
 *   prand = 708194
 *   ah    = 0dfbaa
 *
 * The IRK and the address are stored least significant octet first, so the RPA
 * is 70:81:94:0D:FB:AA, with prand in the upper three octets and the hash in the
 * lower three.
 */
static const uint8_t spec_irk[BT_IRK_SIZE] = {
	0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34,
	0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec,
};

static const bt_addr_t spec_rpa = {{0xaa, 0xfb, 0x0d, 0x94, 0x81, 0x70}};



static void bt_addr_matches_irk__test_spec_sample_data(void **cm_state)
{
	(void)cm_state;
	assert_true(BT_ADDR_IS_RPA(&spec_rpa));
	assert_true(bt_addr_matches_irk(&spec_rpa, spec_irk));
}

static void bt_addr_matches_irk__test_irk_mismatch(void **cm_state)
{
	(void)cm_state;
	uint8_t irk[BT_IRK_SIZE];

	(void)memcpy(irk, spec_irk, sizeof(irk));
	irk[BT_IRK_SIZE - 1U] ^= 0x01;

	assert_false(bt_addr_matches_irk(&spec_rpa, irk));
}

static void bt_addr_matches_irk__test_irk_byte_order(void **cm_state)
{
	(void)cm_state;
	uint8_t irk[BT_IRK_SIZE];

	for (size_t i = 0U; i < BT_IRK_SIZE; i++) {
		irk[i] = spec_irk[BT_IRK_SIZE - 1U - i];
	}

	assert_false(bt_addr_matches_irk(&spec_rpa, irk));
}

static void bt_addr_matches_irk__test_hash_mismatch(void **cm_state)
{
	(void)cm_state;
	bt_addr_t rpa = spec_rpa;

	rpa.val[0] ^= 0x01;

	assert_false(bt_addr_matches_irk(&rpa, spec_irk));
}

static void bt_addr_matches_irk__test_prand_mismatch(void **cm_state)
{
	(void)cm_state;
	bt_addr_t rpa = spec_rpa;

	rpa.val[3] ^= 0x01;

	assert_false(bt_addr_matches_irk(&rpa, spec_irk));
}

static void bt_addr_matches_irk__test_non_rpa_does_not_match(void **cm_state)
{
	(void)cm_state;
	/* The sample prand with each value of the two most significant bits: 0b01 (RPA),
	 * 0b00 (non-resolvable), 0b10 (reserved) and 0b11 (static)
	 */
	const uint8_t prands[][3] = {
		{0x94, 0x81, 0x70},
		{0x94, 0x81, 0x30},
		{0x94, 0x81, 0xb0},
		{0x94, 0x81, 0xf0},
	};
	bt_addr_t addr[ARRAY_SIZE(prands)];

	for (size_t i = 0U; i < ARRAY_SIZE(prands); i++) {
		uint8_t r[16] = {0};
		uint8_t enc[16];

		/* ah(k, r) = e(k, r') mod 2^24, with r' = padding || r */
		(void)memcpy(r, prands[i], sizeof(prands[i]));
		assert_int_equal(bt_encrypt_le(spec_irk, r, enc), 0);
		(void)memcpy(&addr[i].val[0], enc, 3U);
		(void)memcpy(&addr[i].val[3], prands[i], sizeof(prands[i]));
	}

	assert_true(bt_addr_matches_irk(&addr[0], spec_irk));

	for (size_t i = 1U; i < ARRAY_SIZE(prands); i++) {
		assert_false(BT_ADDR_IS_RPA(&addr[i]));
		assert_false(bt_addr_matches_irk(&addr[i], spec_irk));
	}
}





static const struct CMUnitTest test_cases_bt_addr_matches_irk[] = {
	cmocka_unit_test(bt_addr_matches_irk__test_spec_sample_data),
	cmocka_unit_test(bt_addr_matches_irk__test_irk_mismatch),
	cmocka_unit_test(bt_addr_matches_irk__test_irk_byte_order),
	cmocka_unit_test(bt_addr_matches_irk__test_hash_mismatch),
	cmocka_unit_test(bt_addr_matches_irk__test_prand_mismatch),
	cmocka_unit_test(bt_addr_matches_irk__test_non_rpa_does_not_match),
};
BT_TEST_SUITE(bt_addr_matches_irk, test_cases_bt_addr_matches_irk, bt_test_host_setup, NULL);
