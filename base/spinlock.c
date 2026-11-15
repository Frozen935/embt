/****************************************************************************
 *  Copyright (C) 2026 EmBT contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/


/** @file @brief Short internal locks and serialized critical sections. */

#include <embt/base/spinlock.h>
#include <embt/osi/thread.h>
#include "private.h"

bt_spinlock_key_t bt_spin_lock(struct bt_spinlock *lock)
{
	while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
		osi_thread_yield();
	}
	return 0;
}

void bt_spin_unlock(struct bt_spinlock *lock, bt_spinlock_key_t key)
{
	(void)key;
	__atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

void bt_sched_lock(void)
{
	bt_base_require(osi_mutex_lock(&bt_critical_mutex, OS_TIMEOUT_FOREVER));
}

void bt_sched_unlock(void)
{
	bt_base_require(osi_mutex_unlock(&bt_critical_mutex));
}
