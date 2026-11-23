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

/** @file @brief Verify controller crypto command framing and failure ownership. */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <embt/bluetooth/crypto.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/net_buf.h>
#include <embt/base/byteorder.h>
#include <embt/base/time.h>
#include <embt/base/lifo.h>

NET_BUF_POOL_FIXED_DEFINE(pool, 2, 64, 0, NULL);
static const uint8_t key_be[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
static const uint8_t plain_be[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
static const uint8_t encrypted_be[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
	0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};
static uint8_t key_le[16], plain_le[16], encrypted_le[16];
static bool alloc_failure, short_response;
static int command_result;
static unsigned int commands;

struct net_buf *__wrap_bt_hci_cmd_alloc(bt_timeout_t timeout)
{
	(void)timeout;
	return alloc_failure ? NULL : net_buf_alloc(&pool, BT_NO_WAIT);
}

int __wrap_bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf, struct net_buf **rsp)
{
	struct bt_hci_cp_le_encrypt *cp = (void *)buf->data;
	struct bt_hci_rp_le_encrypt *rp;
	assert_int_equal(opcode, BT_HCI_OP_LE_ENCRYPT);
	assert_int_equal(buf->len, sizeof(*cp));
	assert_memory_equal(cp->key, key_le, sizeof(key_le));
	assert_memory_equal(cp->plaintext, plain_le, sizeof(plain_le));
	commands++;
	net_buf_unref(buf);
	if (command_result) {
		return command_result;
	}
	*rsp = net_buf_alloc(&pool, BT_NO_WAIT);
	assert_non_null(*rsp);
	rp = net_buf_add(*rsp, short_response ? 1 : sizeof(*rp));
	rp->status = 0;
	if (!short_response) {
		memcpy(rp->enc_data, encrypted_le, sizeof(encrypted_le));
	}
	return 0;
}

int __wrap_bt_hci_le_rand(void *buffer, size_t len)
{
	memset(buffer, 0x5a, len);
	return command_result;
}

/** @brief LE/BE requests preserve HCI byte order and caller output on failure. */
static void controller_provider(void **state)
{
	uint8_t output[16], original[16];
	(void)state;
	net_buf_pool_init(&pool);
	sys_memcpy_swap(key_le, key_be, 16);
	sys_memcpy_swap(plain_le, plain_be, 16);
	sys_memcpy_swap(encrypted_le, encrypted_be, 16);
	assert_int_equal(bt_encrypt_le(key_le, plain_le, output), 0);
	assert_memory_equal(output, encrypted_le, 16);
	assert_int_equal(bt_encrypt_be(key_be, plain_be, output), 0);
	assert_memory_equal(output, encrypted_be, 16);
	assert_int_equal(bt_rand(output, sizeof(output)), 0);
	assert_int_equal(output[0], 0x5a);
	assert_int_equal(bt_rand(NULL, 16), -EINVAL);
	assert_int_equal(bt_rand(output, 0), -EINVAL);
	assert_int_equal(bt_encrypt_le(NULL, plain_le, output), -EINVAL);
	assert_int_equal(bt_encrypt_be(key_be, NULL, output), -EINVAL);
	memset(output, 0xa5, 16);
	memcpy(original, output, 16);
	alloc_failure = true;
	assert_int_equal(bt_encrypt_le(key_le, plain_le, output), -ENOBUFS);
	alloc_failure = false;
	command_result = -EACCES;
	assert_int_equal(bt_encrypt_be(key_be, plain_be, output), -EACCES);
	assert_memory_equal(output, original, 16);
	command_result = 0;
	short_response = true;
	assert_int_equal(bt_encrypt_le(key_le, plain_le, output), -EMSGSIZE);
	assert_memory_equal(output, original, 16);
	assert_int_equal(commands, 4);
	short_response = false;
	assert_int_equal(bt_encrypt_be(key_be, plain_be, output), 0);
	assert_int_equal(bt_lifo_deinit(&pool.free), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {cmocka_unit_test(controller_provider)};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
