/* long_work.c - Workqueue intended for long-running operations. */

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/base/base.h>
#include <embt/base/work.h>
#include <embt/base/time.h>
#include <string.h>
#include <embt/base/assert.h>
#include <embt/autoconf.h>




static struct bt_work_q bt_long_wq;

int bt_long_wq_schedule(struct bt_work_delayable *dwork, bt_timeout_t timeout)
{
	return bt_work_schedule_for_queue(&bt_long_wq, dwork, timeout);
}

int bt_long_wq_reschedule(struct bt_work_delayable *dwork, bt_timeout_t timeout)
{
	return bt_work_reschedule_for_queue(&bt_long_wq, dwork, timeout);
}

int bt_long_wq_submit(struct bt_work *work)
{
	return bt_work_submit_to_queue(&bt_long_wq, work);
}

int bt_long_wq_init(void)
{
	int err;

	if (bt_long_wq.started) {
		return 0;
	}
	err = bt_work_queue_init(&bt_long_wq);
	if (err) {
		return err;
	}
	err = bt_work_queue_start(&bt_long_wq, CONFIG_BT_LONG_WQ_STACK_SIZE, CONFIG_BT_LONG_WQ_PRIO,
		&(const struct bt_work_queue_config){.name = "BT LW WQ"});
	if (err) {
		bt_base_require(bt_work_queue_deinit(&bt_long_wq));
	}
	return err;
}


int bt_long_wq_deinit(void)
{
	int err = (bt_long_wq.started ? bt_work_queue_drain(&bt_long_wq, true) : 0);
	if (!err && bt_long_wq.joinable) {
		err = bt_work_queue_stop(&bt_long_wq);
	}

	return err ? err : bt_work_queue_deinit(&bt_long_wq);
}
