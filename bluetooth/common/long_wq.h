/* long_wq.h - Workqueue API intended for long-running operations. */

/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <embt/base/work.h>
#include <embt/base/base.h>

int bt_long_wq_schedule(struct bt_work_delayable *dwork, bt_timeout_t timeout);
int bt_long_wq_reschedule(struct bt_work_delayable *dwork, bt_timeout_t timeout);
int bt_long_wq_submit(struct bt_work *dwork);
