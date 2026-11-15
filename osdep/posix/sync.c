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
 * @brief Native POSIX synchronization and bounded semaphore operations.
 */

#include <osi/sync.h>
#include <limits.h>
#include "private.h"

int osi_mutex_init(os_mutex_t *mutex, os_mutex_kind_t kind)
{
	pthread_mutexattr_t attr;
	int rc;

	if (!mutex || (kind != OS_MUTEX_NORMAL && kind != OS_MUTEX_RECURSIVE)) {
		return -EINVAL;
	}
	rc = pthread_mutexattr_init(&attr);
	if (rc != 0) {
		return -rc;
	}
	rc = pthread_mutexattr_settype(&attr, kind == OS_MUTEX_NORMAL ? PTHREAD_MUTEX_NORMAL
								      : PTHREAD_MUTEX_RECURSIVE);
	if (rc == 0) {
		rc = pthread_mutex_init(mutex, &attr);
	}
	pthread_mutexattr_destroy(&attr);
	return -rc;
}

int osi_mutex_deinit(os_mutex_t *mutex)
{
	return mutex ? -pthread_mutex_destroy(mutex) : -EINVAL;
}

int osi_mutex_lock(os_mutex_t *mutex, os_timeout_t timeout_ms)
{
	struct timespec deadline;
	int rc;

	if (!mutex || timeout_ms < OS_TIMEOUT_FOREVER) {
		return -EINVAL;
	}
	if (timeout_ms == OS_TIMEOUT_FOREVER) {
		return -pthread_mutex_lock(mutex);
	}
	if (timeout_ms == OS_TIMEOUT_NO_WAIT) {
		rc = pthread_mutex_trylock(mutex);
		return rc == EBUSY ? -EAGAIN : -rc;
	}
	rc = osi_posix_deadline(timeout_ms, &deadline);
	return rc ? rc : -pthread_mutex_clocklock(mutex, CLOCK_MONOTONIC, &deadline);
}

int osi_mutex_unlock(os_mutex_t *mutex)
{
	return mutex ? -pthread_mutex_unlock(mutex) : -EINVAL;
}

int osi_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t limit)
{
	int rc;

	if (!sem || !limit || limit > SEM_VALUE_MAX || initial_count > limit) {
		return -EINVAL;
	}
	rc = pthread_mutex_init(&sem->give_lock, NULL);
	if (rc != 0) {
		return -rc;
	}
	if (sem_init(&sem->native, 0, initial_count) != 0) {
		rc = errno;
		pthread_mutex_destroy(&sem->give_lock);
		return -rc;
	}
	sem->limit = limit;
	return 0;
}

int osi_sem_deinit(os_sem_t *sem)
{
	if (!sem) {
		return -EINVAL;
	}
	if (sem_destroy(&sem->native) != 0) {
		return -errno;
	}
	return -pthread_mutex_destroy(&sem->give_lock);
}

int osi_sem_take(os_sem_t *sem, os_timeout_t timeout_ms)
{
	struct timespec deadline;
	int rc;

	if (!sem || timeout_ms < OS_TIMEOUT_FOREVER) {
		return -EINVAL;
	}
	if (timeout_ms > 0) {
		rc = osi_posix_deadline(timeout_ms, &deadline);
		if (rc != 0) {
			return rc;
		}
	}
	do {
		if (timeout_ms == OS_TIMEOUT_FOREVER) {
			rc = sem_wait(&sem->native);
		} else if (timeout_ms == OS_TIMEOUT_NO_WAIT) {
			rc = sem_trywait(&sem->native);
		} else {
			rc = sem_clockwait(&sem->native, CLOCK_MONOTONIC, &deadline);
		}
	} while (rc != 0 && errno == EINTR);
	return rc == 0 ? 0 : -errno;
}

int osi_sem_give(os_sem_t *sem)
{
	int count, rc;

	if (!sem) {
		return -EINVAL;
	}
	rc = pthread_mutex_lock(&sem->give_lock);
	if (rc != 0) {
		return -rc;
	}
	/* All increases are serialized; concurrent takes can only reduce the count. */
	if (sem_getvalue(&sem->native, &count) != 0) {
		rc = -errno;
	} else if ((uint32_t)count >= sem->limit) {
		rc = 0;
	} else {
		rc = sem_post(&sem->native) == 0 ? 0 : -errno;
	}
	pthread_mutex_unlock(&sem->give_lock);
	return rc;
}

int osi_sem_count_get(os_sem_t *sem, uint32_t *count)
{
	int value;

	if (!sem || !count) {
		return -EINVAL;
	}
	if (sem_getvalue(&sem->native, &value) != 0) {
		return -errno;
	}
	*count = value > 0 ? (uint32_t)value : 0;
	return 0;
}

int osi_sem_reset(os_sem_t *sem)
{
	if (!sem) {
		return -EINVAL;
	}
	/* The API requires no waiters or concurrent users during this drain. */
	while (sem_trywait(&sem->native) == 0 || errno == EINTR) {
	}
	return errno == EAGAIN ? 0 : -errno;
}

int osi_cond_init(os_cond_t *cond)
{
	pthread_condattr_t attr;
	int rc;

	if (!cond) {
		return -EINVAL;
	}
	rc = pthread_condattr_init(&attr);
	if (rc != 0) {
		return -rc;
	}
	rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	if (rc == 0) {
		rc = pthread_cond_init(cond, &attr);
	}
	pthread_condattr_destroy(&attr);
	return -rc;
}

int osi_cond_deinit(os_cond_t *cond)
{
	return cond ? -pthread_cond_destroy(cond) : -EINVAL;
}

int osi_cond_wait(os_cond_t *cond, os_mutex_t *mutex, os_timeout_t timeout_ms)
{
	struct timespec deadline;
	int rc;

	if (!cond || !mutex || timeout_ms < OS_TIMEOUT_FOREVER) {
		return -EINVAL;
	}
	if (timeout_ms == OS_TIMEOUT_NO_WAIT) {
		return -EAGAIN;
	}
	if (timeout_ms == OS_TIMEOUT_FOREVER) {
		return -pthread_cond_wait(cond, mutex);
	}
	rc = osi_posix_deadline(timeout_ms, &deadline);
	return rc ? rc : -pthread_cond_timedwait(cond, mutex, &deadline);
}

int osi_cond_signal(os_cond_t *cond)
{
	return cond ? -pthread_cond_signal(cond) : -EINVAL;
}

int osi_cond_broadcast(os_cond_t *cond)
{
	return cond ? -pthread_cond_broadcast(cond) : -EINVAL;
}
