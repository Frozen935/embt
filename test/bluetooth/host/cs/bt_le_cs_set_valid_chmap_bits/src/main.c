#include "support/test_support.h"
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <embt/bluetooth/cs.h>
#include <fff.h>

DEFINE_FFF_GLOBALS;



/*
 *  Test uninitialized chmap buffer is populated correctly
 *
 *  Expected behaviour:
 *   - test_chmap matches correct_chmap
 */
static void bt_le_cs_set_valid_chmap_bits__test_uninitialized_chmap(void **cm_state)
{
	(void)cm_state;
	uint8_t test_chmap[10];

	bt_le_cs_set_valid_chmap_bits(test_chmap);

	uint8_t correct_chmap[10] = {0xFC, 0xFF, 0x7F, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F};

	assert_memory_equal(test_chmap, correct_chmap, 10);
}





static const struct CMUnitTest test_cases_bt_le_cs_set_valid_chmap_bits[] = {
	cmocka_unit_test(bt_le_cs_set_valid_chmap_bits__test_uninitialized_chmap),
};
BT_TEST_SUITE(bt_le_cs_set_valid_chmap_bits, test_cases_bt_le_cs_set_valid_chmap_bits, bt_test_base_setup, NULL);
