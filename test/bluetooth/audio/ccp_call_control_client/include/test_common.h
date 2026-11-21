#include "support/test_support.h"
/* test_common.h */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/bap_lc3_preset.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/conn.h>

void test_mocks_init(void);
void test_mocks_cleanup(void);

void test_conn_init(struct bt_conn *conn);
