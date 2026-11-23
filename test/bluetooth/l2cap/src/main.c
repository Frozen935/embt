#include "support/test_support.h"
/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <stddef.h>
#include <embt/bluetooth/buf.h>
#include <embt/bluetooth/bluetooth.h>
#include <embt/bluetooth/l2cap.h>

static int l2cap_accept(struct bt_conn *conn, struct bt_l2cap_server *server,
			struct bt_l2cap_chan **chan)
{
	return -ENOSYS;
}

static struct bt_l2cap_server test_server = {
	.accept		= l2cap_accept,
};

static struct bt_l2cap_server test_fixed_server = {
	.accept		= l2cap_accept,
	.psm		= 0x007f,
};

static struct bt_l2cap_server test_dyn_server = {
	.accept		= l2cap_accept,
	.psm		= 0x00ff,
};

static struct bt_l2cap_server test_inv_server = {
	.accept		= l2cap_accept,
	.psm		= 0xffff,
};

static struct bt_l2cap_server test_unreg_server = {
	.accept		= l2cap_accept,
	.psm		= 0x0071,
};



static void test_l2cap__test_l2cap_register(void **cm_state)
{
	(void)cm_state;
	/* Attempt to register server with PSM auto allocation */
	assert_false(bt_l2cap_server_register(&test_server));

	/* Attempt to register server with fixed PSM */
	assert_false(bt_l2cap_server_register(&test_fixed_server));

	/* Attempt to register server with dynamic PSM */
	assert_false(bt_l2cap_server_register(&test_dyn_server));

	/* Attempt to register server with invalid PSM */
	assert_true(bt_l2cap_server_register(&test_inv_server));

	/* Attempt to re-register server with PSM auto allocation */
	assert_int_equal(bt_l2cap_server_register(&test_server), -EALREADY);

	/* Attempt to re-register server with fixed PSM */
	assert_int_equal(bt_l2cap_server_register(&test_fixed_server), -EALREADY);

	/* Attempt to re-register server with dynamic PSM */
	assert_int_equal(bt_l2cap_server_register(&test_dyn_server), -EALREADY);
}

static void test_l2cap__test_l2cap_unregister(void **cm_state)
{
	(void)cm_state;
	/* Attempt to unregister a NULL server */
	assert_int_equal(bt_l2cap_server_unregister(NULL), -EINVAL);

	/* Attempt to unregister a server that was never registered */
	assert_int_equal(bt_l2cap_server_unregister(&test_unreg_server), -ENOENT);

	assert_int_equal(bt_l2cap_server_register(&test_unreg_server), 0);

	/* The server cannot be registered twice */
	assert_int_equal(bt_l2cap_server_register(&test_unreg_server), -EALREADY);

	assert_int_equal(bt_l2cap_server_unregister(&test_unreg_server), 0);

	/* Attempt to unregister the same server twice */
	assert_int_equal(bt_l2cap_server_unregister(&test_unreg_server), -ENOENT);

	/* The PSM is available again */
	assert_int_equal(bt_l2cap_server_register(&test_unreg_server), 0);
	assert_int_equal(bt_l2cap_server_unregister(&test_unreg_server), 0);
}





static const struct CMUnitTest test_cases_test_l2cap[] = {
	cmocka_unit_test(test_l2cap__test_l2cap_register),
	cmocka_unit_test(test_l2cap__test_l2cap_unregister),
};
BT_TEST_SUITE(test_l2cap, test_cases_test_l2cap, bt_test_host_setup, NULL);
