#include "support/test_support.h"
#include "test_environment.h"
void mock_init_rule_before_744017(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_744017(const struct bt_test_case_info *, void *);
void mock_init_rule_before_127f12(const struct bt_test_case_info *, void *);
void mock_destroy_rule_after_127f12(const struct bt_test_case_info *, void *);


/* cancel.c - unit test for bt_cap_handover_cancel */

/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <embt/autoconf.h>
#include <embt/bluetooth/addr.h>
#include <embt/bluetooth/audio/bap.h>
#include <embt/bluetooth/audio/cap.h>
#include <embt/bluetooth/hci_types.h>
#include <fff.h>
#include <embt/base/atomic.h>
#include <embt/base/util.h>
#include <embt/base/util_macro.h>
#include "audio/cap_internal.h"
#include "cap_handover.h"
#include "expects_util.h"
#include "test_common.h"

#define FFF_GLOBALS

struct cap_handover_test_cancel_fixture {
	struct bt_cap_common_proc *active_proc;
};

static void *cap_handover_test_cancel_setup(void)
{
	struct cap_handover_test_cancel_fixture *fixture;

	fixture = malloc(sizeof(*fixture));
	assert_non_null(fixture);

	return fixture;
}

static void cap_handover_test_cancel_before(void *f)
{
	struct cap_handover_test_cancel_fixture *fixture = f;
	int err;

	(void)memset(f, 0, sizeof(struct cap_handover_test_cancel_fixture));

	fixture->active_proc = bt_cap_common_get_active_proc();

	bt_atomic_set_bit(fixture->active_proc->proc_state_flags, BT_CAP_COMMON_PROC_STATE_ACTIVE);
	bt_atomic_set_bit(fixture->active_proc->proc_state_flags, BT_CAP_COMMON_PROC_STATE_HANDOVER);

	bt_cap_common_unlock_proc();

	err = bt_cap_handover_register_cb(&mock_cap_handover_cb);
	assert_int_equal(0, err);
}

static void cap_handover_test_cancel_after(void *f)
{
	struct cap_handover_test_cancel_fixture *fixture = f;
	int err;

	(void)memset(fixture->active_proc, 0, sizeof(*fixture->active_proc));

	err = bt_cap_handover_unregister_cb(&mock_cap_handover_cb);
	assert_true(err == 0);
}

static void cap_handover_test_cancel_teardown(void *f)
{
	free(f);
}

static void test_cancel(struct cap_handover_test_cancel_fixture *fixture)
{
	/* Get this value before cancel, as cancel will clear the active proc */
	const bool is_unicast_to_broadcast =
		fixture->active_proc->proc_param.handover.is_unicast_to_broadcast;
	int err;

	err = bt_cap_handover_cancel();
	assert_int_equal(0, err);

	if (is_unicast_to_broadcast) {
		zexpect_call_count("bt_cap_handover_cb.unicast_to_broadcast_complete", 1,
				   mock_unicast_to_broadcast_complete_cb_fake.call_count);
		assert_int_equal(-ECANCELED, mock_unicast_to_broadcast_complete_cb_fake.arg0_history[0]);
	} else {
		zexpect_call_count("bt_cap_handover_cb.broadcast_to_unicast_complete", 1,
				   mock_broadcast_to_unicast_complete_cb_fake.call_count);
		assert_int_equal(-ECANCELED, mock_broadcast_to_unicast_complete_cb_fake.arg0_history[0]);
	}
}



static void cap_handover_test_cancel__test_handover_cancel_broadcast_to_unicast(void **cm_state)
{
	struct cap_handover_test_cancel_fixture *fixture = *cm_state;
	fixture->active_proc->proc_param.handover.is_unicast_to_broadcast = false;
	test_cancel(fixture);
}

static void cap_handover_test_cancel__test_handover_cancel_unicast_to_broadcast(void **cm_state)
{
	struct cap_handover_test_cancel_fixture *fixture = *cm_state;
	fixture->active_proc->proc_param.handover.is_unicast_to_broadcast = true;
	test_cancel(fixture);
}

static void cap_handover_test_cancel__test_handover_cancel_double(void **cm_state)
{
	struct cap_handover_test_cancel_fixture *fixture = *cm_state;
	int err;

	test_cancel(fixture);

	err = bt_cap_handover_cancel();
	assert_int_equal(-EALREADY, err);
}

static void cap_handover_test_cancel__test_handover_cancel_not_active(void **cm_state)
{
	struct cap_handover_test_cancel_fixture *fixture = *cm_state;
	int err;

	bt_atomic_clear_bit(fixture->active_proc->proc_state_flags, BT_CAP_COMMON_PROC_STATE_ACTIVE);

	err = bt_cap_handover_cancel();
	assert_int_equal(-EALREADY, err);
}

static void cap_handover_test_cancel__test_handover_cancel_not_handover(void **cm_state)
{
	struct cap_handover_test_cancel_fixture *fixture = *cm_state;
	int err;

	bt_atomic_clear_bit(fixture->active_proc->proc_state_flags, BT_CAP_COMMON_PROC_STATE_HANDOVER);

	err = bt_cap_handover_cancel();
	assert_int_equal(-EOPNOTSUPP, err);
}

int bt_test_cap_handover_test_cancel_setup(void **state)
{
	int environment_result = bt_test_audio_cap_handover_environment_setup(state);

	if (environment_result != 0) {
		return environment_result;
	}

	*state = cap_handover_test_cancel_setup();
	return 0;
}
static int bt_test_cap_handover_test_cancel_before(void **state)
{
	(void)state;
	cap_handover_test_cancel_before(*state);
	mock_init_rule_before_744017(NULL, *state);
	mock_init_rule_before_127f12(NULL, *state);
	return 0;
}
int bt_test_cap_handover_test_cancel_after(void **state)
{
	(void)state;
	cap_handover_test_cancel_after(*state);
	mock_destroy_rule_after_744017(NULL, *state);
	mock_destroy_rule_after_127f12(NULL, *state);
	return 0;
}
int bt_test_cap_handover_test_cancel_teardown(void **state)
{
	(void)state;
	cap_handover_test_cancel_teardown(*state);
	return 0;
}
static const struct CMUnitTest test_cases_cap_handover_test_cancel[] = {
	cmocka_unit_test_setup_teardown(cap_handover_test_cancel__test_handover_cancel_broadcast_to_unicast, bt_test_cap_handover_test_cancel_before, bt_test_cap_handover_test_cancel_after),
	cmocka_unit_test_setup_teardown(cap_handover_test_cancel__test_handover_cancel_unicast_to_broadcast, bt_test_cap_handover_test_cancel_before, bt_test_cap_handover_test_cancel_after),
	cmocka_unit_test_setup_teardown(cap_handover_test_cancel__test_handover_cancel_double, bt_test_cap_handover_test_cancel_before, bt_test_cap_handover_test_cancel_after),
	cmocka_unit_test_setup_teardown(cap_handover_test_cancel__test_handover_cancel_not_active, bt_test_cap_handover_test_cancel_before, bt_test_cap_handover_test_cancel_after),
	cmocka_unit_test_setup_teardown(cap_handover_test_cancel__test_handover_cancel_not_handover, bt_test_cap_handover_test_cancel_before, bt_test_cap_handover_test_cancel_after),
};
BT_TEST_SUITE(cap_handover_test_cancel, test_cases_cap_handover_test_cancel, bt_test_cap_handover_test_cancel_setup, bt_test_cap_handover_test_cancel_teardown);
