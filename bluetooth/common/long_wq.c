/* long_work.c - Workqueue intended for long-running operations. */

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <base/base.h>
#include <embt/autoconf.h>



#include <embt/storage.h>

static struct k_work_q bt_long_wq;

int bt_long_wq_schedule(struct k_work_delayable *dwork, k_timeout_t timeout)
{
	return k_work_schedule_for_queue(&bt_long_wq, dwork, timeout);
}

int bt_long_wq_reschedule(struct k_work_delayable *dwork, k_timeout_t timeout)
{
	return k_work_reschedule_for_queue(&bt_long_wq, dwork, timeout);
}

int bt_long_wq_submit(struct k_work *work)
{
	return k_work_submit_to_queue(&bt_long_wq, work);
}

int bt_long_wq_init(void)
{

	return bt_long_wq.runtime ? 0 : bt_work_queue_start(&bt_long_wq, "BT LW WQ", 0);
}


int bt_long_wq_deinit(void)
{
	return bt_work_queue_stop(&bt_long_wq);
}
