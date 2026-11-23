#include "support/test_support.h"
/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <embt/bluetooth/hci.h>
#include "host/hci_core.h"
#include "mocks/hci_core.h"

struct bt_dev bt_dev = {
	.manufacturer = 0x1234,
};

DEFINE_FAKE_VALUE_FUNC(int, bt_hci_le_rand, void *, size_t);
