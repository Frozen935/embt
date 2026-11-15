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

/**
 * @file
 * @brief Short locks and shared critical sections.
 */

#ifndef EMBT_BASE_SPINLOCK_H_
#define EMBT_BASE_SPINLOCK_H_



struct bt_spinlock {
	unsigned char locked;
};

typedef unsigned int bt_spinlock_key_t;
/**
 * @brief Acquire a short internal lock with acquire ordering.
 *
 * Do not sleep, invoke callbacks, or acquire this lock recursively while holding it.
 *
 * @param[in,out] lock Naturally aligned lock storage.
 * @return Token for the matching unlock.
 */
bt_spinlock_key_t bt_spin_lock(struct bt_spinlock *lock);
/**
 * @brief Release a short internal lock with release ordering.
 *
 * @param[in,out] lock Naturally aligned lock storage.
 * @param[in] key Token returned by the matching lock call.
 */
void bt_spin_unlock(struct bt_spinlock *lock, bt_spinlock_key_t key);
/**
 * @brief Acquire the shared recursive critical-section mutex.
 *
 * This protects only participating callers; it does not disable POSIX scheduling.
 */
void bt_sched_lock(void);
/**
 * @brief Release one shared critical-section acquisition.
 */
void bt_sched_unlock(void);

#endif /* EMBT_BASE_SPINLOCK_H_ */
