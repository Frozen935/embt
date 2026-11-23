/* Copyright (c) 2022 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <embt/base/assert.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/uuid.h>

/* NOTE: These helper functions encode into the same per-thread buffer storage.
 * It is the responsibility of the user of this function to copy the information
 * in this string if needed.
 *
 * NOTE: A subsequent call on the same thread overwrites its previous result.
 */
const char *bt_hex(const void *buf, size_t len);
const char *bt_uuid_str(const struct bt_uuid *uuid);
