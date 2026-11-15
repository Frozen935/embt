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

/** @file @brief Controller-provided random data and AES encryption over HCI. */
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <embt/bluetooth/crypto.h>
#include <embt/bluetooth/hci_types.h>
#include <embt/base/byteorder.h>
#include <embt/base/time.h>
#include <embt/net_buf.h>
#include "hci_core.h"

int bt_rand(void *buf, size_t len)
{
	if (buf == NULL || len == 0) {
		return -EINVAL;
	}

	return bt_hci_le_rand(buf, len);
}

int bt_encrypt_le(const uint8_t key[16], const uint8_t plaintext[16], uint8_t enc_data[16])
{
	struct bt_hci_cp_le_encrypt *cp;
	struct bt_hci_rp_le_encrypt *rp;
	struct net_buf *buf, *rsp;
	int err;

	if (key == NULL || plaintext == NULL || enc_data == NULL) {
		return -EINVAL;
	}
	buf = bt_hci_cmd_alloc(BT_FOREVER);
	if (buf == NULL) {
		return -ENOBUFS;
	}
	cp = net_buf_add(buf, sizeof(*cp));
	memcpy(cp->key, key, sizeof(cp->key));
	memcpy(cp->plaintext, plaintext, sizeof(cp->plaintext));
	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_ENCRYPT, buf, &rsp);
	if (err) {
		return err;
	}
	rp = (void *)rsp->data;
	if (rsp->len < sizeof(*rp)) {
		err = -EMSGSIZE;
	} else {
		memcpy(enc_data, rp->enc_data, sizeof(rp->enc_data));
	}
	net_buf_unref(rsp);
	return err;
}

int bt_encrypt_be(const uint8_t key[16], const uint8_t plaintext[16], uint8_t enc_data[16])
{
	uint8_t key_le[16], plaintext_le[16], encrypted_le[16];
	int err;

	if (key == NULL || plaintext == NULL || enc_data == NULL) {
		return -EINVAL;
	}
	sys_memcpy_swap(key_le, key, sizeof(key_le));
	sys_memcpy_swap(plaintext_le, plaintext, sizeof(plaintext_le));
	err = bt_encrypt_le(key_le, plaintext_le, encrypted_le);
	if (!err) {
		sys_memcpy_swap(enc_data, encrypted_le, sizeof(encrypted_le));
	}
	return err;
}
