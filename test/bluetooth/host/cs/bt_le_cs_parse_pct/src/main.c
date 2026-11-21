#include "support/test_support.h"
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <embt/bluetooth/cs.h>
#include <fff.h>
#include <embt/base/log.h>

DEFINE_FFF_GLOBALS;



struct pct_test_vector {
	uint8_t input[3];
	struct bt_le_cs_iq_sample output;
};

static const struct pct_test_vector test_vector[] = {
	/* Edge cases */
	{.input = {0x00, 0x00, 0x00}, .output = {.i = 0, .q = 0}},
	{.input = {0xFF, 0xFF, 0xFF}, .output = {.i = -1, .q = -1}},
	{.input = {0xFF, 0x00, 0xFF}, .output = {.i = 255, .q = -16}},
	{.input = {0xFF, 0x00, 0x00}, .output = {.i = 255, .q = 0}},
	{.input = {0x00, 0xFF, 0x00}, .output = {.i = -256, .q = 15}},
	{.input = {0x00, 0x00, 0xFF}, .output = {.i = 0, .q = -16}},
	{.input = {0x00, 0x08, 0x80}, .output = {.i = -2048, .q = -2048}},
	{.input = {0xFF, 0xF7, 0x7F}, .output = {.i = 2047, .q = 2047}},

	/* Randomly generated using python */
	{.input = {0xEF, 0xCD, 0xAB}, .output = {.i = -529, .q = -1348}},
	{.input = {0x30, 0x75, 0x44}, .output = {.i = 1328, .q = 1095}},
	{.input = {0x46, 0x5D, 0xEB}, .output = {.i = -698, .q = -331}},
	{.input = {0xE8, 0x14, 0x45}, .output = {.i = 1256, .q = 1105}},
	{.input = {0x23, 0xCA, 0x5C}, .output = {.i = -1501, .q = 1484}},
	{.input = {0x68, 0xA0, 0x15}, .output = {.i = 104, .q = 346}},
	{.input = {0x39, 0x73, 0x1B}, .output = {.i = 825, .q = 439}},
	{.input = {0x23, 0x72, 0x3D}, .output = {.i = 547, .q = 983}},
	{.input = {0xF5, 0xF8, 0x3D}, .output = {.i = -1803, .q = 991}},
	{.input = {0xF7, 0xB4, 0xB9}, .output = {.i = 1271, .q = -1125}},
	{.input = {0x61, 0x9F, 0xD5}, .output = {.i = -159, .q = -679}},
	{.input = {0x9B, 0x21, 0xC6}, .output = {.i = 411, .q = -926}},
	{.input = {0x14, 0x86, 0x0F}, .output = {.i = 1556, .q = 248}},
	{.input = {0x8E, 0xBB, 0xC6}, .output = {.i = -1138, .q = -917}},
	{.input = {0x5B, 0xD1, 0xC2}, .output = {.i = 347, .q = -979}},
	{.input = {0x99, 0x4A, 0x28}, .output = {.i = -1383, .q = 644}},
	{.input = {0x32, 0x16, 0x2B}, .output = {.i = 1586, .q = 689}},
	{.input = {0x3E, 0x8C, 0xD4}, .output = {.i = -962, .q = -696}},
	{.input = {0x2B, 0x1F, 0x95}, .output = {.i = -213, .q = -1711}},
	{.input = {0x22, 0xE6, 0xD6}, .output = {.i = 1570, .q = -658}},
	{.input = {0x0B, 0x31, 0xD6}, .output = {.i = 267, .q = -669}},
	{.input = {0x1B, 0x98, 0x9D}, .output = {.i = -2021, .q = -1575}},
	{.input = {0x8E, 0x97, 0x63}, .output = {.i = 1934, .q = 1593}},
	{.input = {0x97, 0x91, 0x8D}, .output = {.i = 407, .q = -1831}},
	{.input = {0x67, 0xF7, 0x1F}, .output = {.i = 1895, .q = 511}},
	{.input = {0xD6, 0x5C, 0x23}, .output = {.i = -810, .q = 565}},
	{.input = {0x92, 0xD3, 0x0B}, .output = {.i = 914, .q = 189}},
	{.input = {0xE8, 0xF3, 0x23}, .output = {.i = 1000, .q = 575}},
	{.input = {0xE6, 0xE3, 0xAD}, .output = {.i = 998, .q = -1314}},
	{.input = {0x6E, 0x70, 0xA9}, .output = {.i = 110, .q = -1385}},
	{.input = {0x63, 0x65, 0x28}, .output = {.i = 1379, .q = 646}},
	{.input = {0x27, 0x0F, 0x32}, .output = {.i = -217, .q = 800}},
	{.input = {0x3F, 0x8C, 0xE1}, .output = {.i = -961, .q = -488}},
	{.input = {0x4E, 0x86, 0xAA}, .output = {.i = 1614, .q = -1368}},
	{.input = {0x9E, 0xD1, 0xF6}, .output = {.i = 414, .q = -147}},
	{.input = {0x86, 0x09, 0x56}, .output = {.i = -1658, .q = 1376}},
	{.input = {0xFF, 0x09, 0x41}, .output = {.i = -1537, .q = 1040}},
	{.input = {0x89, 0xC5, 0x1F}, .output = {.i = 1417, .q = 508}},
	{.input = {0x1A, 0xE2, 0x9A}, .output = {.i = 538, .q = -1618}},
	{.input = {0x7E, 0x03, 0xB8}, .output = {.i = 894, .q = -1152}},
	{.input = {0x5E, 0x28, 0xB3}, .output = {.i = -1954, .q = -1230}},
	{.input = {0xFF, 0x50, 0xF0}, .output = {.i = 255, .q = -251}},
	{.input = {0xB0, 0x07, 0x87}, .output = {.i = 1968, .q = -1936}},
	{.input = {0x7E, 0xD7, 0x0C}, .output = {.i = 1918, .q = 205}},
	{.input = {0x26, 0xA2, 0xC9}, .output = {.i = 550, .q = -870}},
	{.input = {0x97, 0x71, 0x72}, .output = {.i = 407, .q = 1831}},
	{.input = {0x73, 0x0E, 0xC1}, .output = {.i = -397, .q = -1008}},
	{.input = {0xAC, 0x20, 0x6B}, .output = {.i = 172, .q = 1714}},
	{.input = {0x85, 0x7D, 0xB4}, .output = {.i = -635, .q = -1209}},
	{.input = {0xCC, 0xE3, 0x1B}, .output = {.i = 972, .q = 446}},
	{.input = {0x88, 0x48, 0x65}, .output = {.i = -1912, .q = 1620}},
};

/* Human-readable per-case label from the raw PCT bytes, e.g. "0xEFCDAB". */
static const char *pct_vector_name(size_t index, const void *value)
{
	const struct pct_test_vector *v = value;
	static char buf[16];

	ARG_UNUSED(index);
	snprintf(buf, sizeof(buf), "0x%02X%02X%02X",
		 v->input[0], v->input[1], v->input[2]);
	return buf;
}



/*
 *  Test success case
 *
 *  Constraints:
 *   - Valid PCT is passed in
 *
 *  Expected behaviour:
 *   - IQ term matches expected values
 */
static void bt_le_cs_parse_pct__test_parsing_success(void **cm_state)
{
	const struct pct_test_vector *v = *cm_state;
	struct bt_le_cs_iq_sample iq;

	iq = bt_le_cs_parse_pct(v->input);

	assert_int_equal(iq.i, v->output.i);
	assert_int_equal(iq.q, v->output.q);
}







static const struct CMUnitTest test_cases_bt_le_cs_parse_pct[] = {
	{"bt_le_cs_parse_pct__test_parsing_success_0", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[0]},
	{"bt_le_cs_parse_pct__test_parsing_success_1", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[1]},
	{"bt_le_cs_parse_pct__test_parsing_success_2", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[2]},
	{"bt_le_cs_parse_pct__test_parsing_success_3", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[3]},
	{"bt_le_cs_parse_pct__test_parsing_success_4", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[4]},
	{"bt_le_cs_parse_pct__test_parsing_success_5", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[5]},
	{"bt_le_cs_parse_pct__test_parsing_success_6", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[6]},
	{"bt_le_cs_parse_pct__test_parsing_success_7", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[7]},
	{"bt_le_cs_parse_pct__test_parsing_success_8", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[8]},
	{"bt_le_cs_parse_pct__test_parsing_success_9", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[9]},
	{"bt_le_cs_parse_pct__test_parsing_success_10", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[10]},
	{"bt_le_cs_parse_pct__test_parsing_success_11", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[11]},
	{"bt_le_cs_parse_pct__test_parsing_success_12", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[12]},
	{"bt_le_cs_parse_pct__test_parsing_success_13", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[13]},
	{"bt_le_cs_parse_pct__test_parsing_success_14", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[14]},
	{"bt_le_cs_parse_pct__test_parsing_success_15", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[15]},
	{"bt_le_cs_parse_pct__test_parsing_success_16", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[16]},
	{"bt_le_cs_parse_pct__test_parsing_success_17", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[17]},
	{"bt_le_cs_parse_pct__test_parsing_success_18", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[18]},
	{"bt_le_cs_parse_pct__test_parsing_success_19", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[19]},
	{"bt_le_cs_parse_pct__test_parsing_success_20", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[20]},
	{"bt_le_cs_parse_pct__test_parsing_success_21", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[21]},
	{"bt_le_cs_parse_pct__test_parsing_success_22", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[22]},
	{"bt_le_cs_parse_pct__test_parsing_success_23", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[23]},
	{"bt_le_cs_parse_pct__test_parsing_success_24", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[24]},
	{"bt_le_cs_parse_pct__test_parsing_success_25", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[25]},
	{"bt_le_cs_parse_pct__test_parsing_success_26", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[26]},
	{"bt_le_cs_parse_pct__test_parsing_success_27", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[27]},
	{"bt_le_cs_parse_pct__test_parsing_success_28", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[28]},
	{"bt_le_cs_parse_pct__test_parsing_success_29", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[29]},
	{"bt_le_cs_parse_pct__test_parsing_success_30", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[30]},
	{"bt_le_cs_parse_pct__test_parsing_success_31", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[31]},
	{"bt_le_cs_parse_pct__test_parsing_success_32", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[32]},
	{"bt_le_cs_parse_pct__test_parsing_success_33", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[33]},
	{"bt_le_cs_parse_pct__test_parsing_success_34", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[34]},
	{"bt_le_cs_parse_pct__test_parsing_success_35", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[35]},
	{"bt_le_cs_parse_pct__test_parsing_success_36", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[36]},
	{"bt_le_cs_parse_pct__test_parsing_success_37", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[37]},
	{"bt_le_cs_parse_pct__test_parsing_success_38", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[38]},
	{"bt_le_cs_parse_pct__test_parsing_success_39", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[39]},
	{"bt_le_cs_parse_pct__test_parsing_success_40", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[40]},
	{"bt_le_cs_parse_pct__test_parsing_success_41", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[41]},
	{"bt_le_cs_parse_pct__test_parsing_success_42", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[42]},
	{"bt_le_cs_parse_pct__test_parsing_success_43", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[43]},
	{"bt_le_cs_parse_pct__test_parsing_success_44", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[44]},
	{"bt_le_cs_parse_pct__test_parsing_success_45", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[45]},
	{"bt_le_cs_parse_pct__test_parsing_success_46", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[46]},
	{"bt_le_cs_parse_pct__test_parsing_success_47", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[47]},
	{"bt_le_cs_parse_pct__test_parsing_success_48", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[48]},
	{"bt_le_cs_parse_pct__test_parsing_success_49", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[49]},
	{"bt_le_cs_parse_pct__test_parsing_success_50", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[50]},
	{"bt_le_cs_parse_pct__test_parsing_success_51", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[51]},
	{"bt_le_cs_parse_pct__test_parsing_success_52", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[52]},
	{"bt_le_cs_parse_pct__test_parsing_success_53", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[53]},
	{"bt_le_cs_parse_pct__test_parsing_success_54", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[54]},
	{"bt_le_cs_parse_pct__test_parsing_success_55", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[55]},
	{"bt_le_cs_parse_pct__test_parsing_success_56", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[56]},
	{"bt_le_cs_parse_pct__test_parsing_success_57", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[57]},
	{"bt_le_cs_parse_pct__test_parsing_success_58", bt_le_cs_parse_pct__test_parsing_success, NULL, NULL, (void *)&test_vector[58]}
};
BT_TEST_SUITE(bt_le_cs_parse_pct, test_cases_bt_le_cs_parse_pct, bt_test_base_setup, NULL);
