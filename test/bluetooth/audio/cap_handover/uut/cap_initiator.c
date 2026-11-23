#include "support/test_support.h"
/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/audio/csip.h>
#include <embt/bluetooth/bluetooth.h>
#include <fff.h>

#include "cap_initiator.h"

/* List of fakes used by this unit tester */
#define FFF_FAKES_LIST(FAKE)                                                                       \
	FAKE(mock_unicast_start_complete_cb)                                                       \
	FAKE(mock_broadcast_start_cb)

DEFINE_FAKE_VOID_FUNC(mock_unicast_start_complete_cb, int, struct bt_conn *);
DEFINE_FAKE_VOID_FUNC(mock_broadcast_start_cb, struct bt_cap_broadcast_source *);

const struct bt_cap_initiator_cb mock_cap_initiator_cb = {
	.unicast_start_complete = mock_unicast_start_complete_cb,
	.broadcast_started = mock_broadcast_start_cb,
};

void mock_cap_initiator_init(void)
{
	FFF_FAKES_LIST(RESET_FAKE);
}
