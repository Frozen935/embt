#include "support/test_support.h"
/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mocks/hci_core.h"

#include <embt/bluetooth/hci.h>


DEFINE_FAKE_VALUE_FUNC(struct net_buf *, bt_hci_cmd_alloc, bt_timeout_t);
DEFINE_FAKE_VALUE_FUNC(int, bt_hci_cmd_send_sync, uint16_t, struct net_buf *, struct net_buf **);
