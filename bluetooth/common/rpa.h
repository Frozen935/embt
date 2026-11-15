/* rpa.h - Bluetooth Resolvable Private Addresses (RPA) generation and
 * resolution
 */

/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <base/base.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/hci.h>

int bt_rpa_create(const uint8_t irk[16], bt_addr_t *rpa);
