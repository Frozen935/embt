/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_HCI_RAW_INTERNAL_H
#define __BT_HCI_RAW_INTERNAL_H

#include <embt/drivers/bluetooth.h>
#include <embt/net_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_dev_raw {
	const struct bt_hci_transport *hci;
};

extern struct bt_dev_raw bt_dev;

/**
 * @brief Initialize raw HCI pools before their first allocation.
 * @note The lifecycle owner serializes initialization calls.
 */
void bt_hci_raw_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BT_HCI_RAW_INTERNAL_H */
