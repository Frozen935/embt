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
 * @brief Native error wrappers with thread-local selection and allocation accounting.
 */

#include "test_support.h"
#include "faults.h"
#include <stdlib.h>
#include <signal.h>
static _Thread_local enum test_fault selected;
static _Thread_local int error_code;
atomic_int test_live_allocations;
atomic_uint test_attr_destroyed, test_mutexattr_destroyed;
atomic_uint test_condattr_destroyed, test_mutex_destroyed;

void test_fault_arm(enum test_fault fault, int code)
{
	selected = fault;
	error_code = code;
}
/**
 * @brief Consume the calling thread's matching one-shot failure.
 * @param[in] fault Native operation being intercepted.
 * @return Selected positive error code, or zero when inactive.
 */
static int consume(enum test_fault fault)
{
	if (selected != fault) {
		return 0;
	}
	selected = F_NONE;
	return error_code;
}
void *__real_malloc(size_t size);
/**
 * @brief Intercept malloc for controlled test failures.
 * @param[in] size Allocation size in bytes.
 * @return Native allocation result or injected NULL.
 */
void *__wrap_malloc(size_t size)
{
	void *memory = consume(F_MALLOC) ? NULL : __real_malloc(size);
	if (memory) {
		atomic_fetch_add(&test_live_allocations, 1);
	}
	return memory;
}
void *__real_calloc(size_t count, size_t size);
/**
 * @brief Intercept calloc for controlled test failures.
 * @param[in] count Number of elements.
 * @param[in] size Element size in bytes.
 * @return Native allocation result or injected NULL.
 */
void *__wrap_calloc(size_t count, size_t size)
{
	void *memory = consume(F_CALLOC) ? NULL : __real_calloc(count, size);
	if (memory) {
		atomic_fetch_add(&test_live_allocations, 1);
	}
	return memory;
}
int __real_posix_memalign(void **memory, size_t alignment, size_t size);
/**
 * @brief Intercept posix_memalign for controlled test failures.
 * @param[out] memory Native allocation output.
 * @param[in] alignment Requested native alignment.
 * @param[in] size Allocation size in bytes.
 * @return Native return code or injected positive error.
 */
int __wrap_posix_memalign(void **memory, size_t alignment, size_t size)
{
	int rc = consume(F_ALIGN);
	if (!rc) {
		rc = __real_posix_memalign(memory, alignment, size);
		if (!rc) {
			atomic_fetch_add(&test_live_allocations, 1);
		}
	}
	return rc;
}
void __real_free(void *memory);
/**
 * @brief Intercept free for controlled test failures.
 * @param[in] memory Allocation to release, or NULL.
 */
void __wrap_free(void *memory)
{
	if (memory) {
		atomic_fetch_sub(&test_live_allocations, 1);
	}
	__real_free(memory);
}
#define WRAP_RC(name, fault, declaration, arguments)                                               \
	int __real_##name declaration;                                                             \
	int __wrap_##name declaration                                                              \
	{                                                                                          \
		int rc = consume(fault);                                                           \
		return rc ? rc : __real_##name arguments;                                          \
	}
WRAP_RC(pthread_create, F_CREATE,
	(pthread_t * thread, const pthread_attr_t *attr, void *(*entry)(void *), void *argument),
	(thread, attr, entry, argument))
WRAP_RC(pthread_attr_init, F_ATTR_INIT, (pthread_attr_t * attr), (attr))
WRAP_RC(pthread_mutexattr_init, F_MUTEX_ATTR_INIT, (pthread_mutexattr_t * attr), (attr))
WRAP_RC(pthread_mutexattr_settype, F_MUTEX_ATTR_TYPE, (pthread_mutexattr_t * attr, int type),
	(attr, type))
WRAP_RC(pthread_mutex_init, F_MUTEX_INIT,
	(pthread_mutex_t * mutex, const pthread_mutexattr_t *attr), (mutex, attr))
WRAP_RC(pthread_condattr_init, F_COND_ATTR_INIT, (pthread_condattr_t * attr), (attr))
WRAP_RC(pthread_condattr_setclock, F_COND_ATTR_CLOCK, (pthread_condattr_t * attr, clockid_t clock),
	(attr, clock))
WRAP_RC(pthread_cond_init, F_COND_INIT, (pthread_cond_t * cond, const pthread_condattr_t *attr),
	(cond, attr))
WRAP_RC(clock_nanosleep, F_SLEEP,
	(clockid_t clock, int flags, const struct timespec *requested, struct timespec *remaining),
	(clock, flags, requested, remaining))
#define WRAP_DESTROY(name, type, counter)                                                          \
	int __real_##name(type *object);                                                           \
	int __wrap_##name(type *object)                                                            \
	{                                                                                          \
		atomic_fetch_add(&counter, 1);                                                     \
		return __real_##name(object);                                                      \
	}
WRAP_DESTROY(pthread_attr_destroy, pthread_attr_t, test_attr_destroyed)
WRAP_DESTROY(pthread_mutexattr_destroy, pthread_mutexattr_t, test_mutexattr_destroyed)
WRAP_DESTROY(pthread_condattr_destroy, pthread_condattr_t, test_condattr_destroyed)
WRAP_DESTROY(pthread_mutex_destroy, pthread_mutex_t, test_mutex_destroyed)
#define WRAP_ERRNO(name, fault, declaration, arguments)                                            \
	int __real_##name declaration;                                                             \
	int __wrap_##name declaration                                                              \
	{                                                                                          \
		int code = consume(fault);                                                         \
		if (code) {                                                                        \
			errno = code;                                                              \
			return -1;                                                                 \
		}                                                                                  \
		return __real_##name arguments;                                                    \
	}
WRAP_ERRNO(sem_init, F_SEM_INIT, (sem_t * sem, int shared, unsigned int value),
	   (sem, shared, value))
WRAP_ERRNO(sem_getvalue, F_SEM_VALUE, (sem_t * sem, int *value), (sem, value))
WRAP_ERRNO(sem_post, F_SEM_POST, (sem_t * sem), (sem))
WRAP_ERRNO(sem_clockwait, F_SEM_WAIT,
	   (sem_t * sem, clockid_t clock, const struct timespec *deadline), (sem, clock, deadline))

int __real_clock_gettime(clockid_t clock, struct timespec *value);
/**
 * @brief Intercept clock_gettime for controlled test failures.
 * @param[in] clock Native clock identifier.
 * @param[out] value Native timestamp output.
 * @return Native zero/-1 result, with errno for failures.
 */
int __wrap_clock_gettime(clockid_t clock, struct timespec *value)
{
	if (selected == F_CLOCK_LIMIT) {
		selected = F_NONE;
		value->tv_sec = sizeof(time_t) == 8 ? INT64_MAX : INT32_MAX;
		value->tv_nsec = 999999999;
		return 0;
	}
	int code = consume(F_CLOCK);
	if (code) {
		errno = code;
		return -1;
	}
	return __real_clock_gettime(clock, value);
}
WRAP_ERRNO(timer_create, F_TIMER_CREATE, (clockid_t clock, struct sigevent *event, timer_t *timer),
	   (clock, event, timer))
WRAP_ERRNO(timer_settime, F_TIMER_SET,
	   (timer_t timer, int flags, const struct itimerspec *value, struct itimerspec *previous),
	   (timer, flags, value, previous))
WRAP_ERRNO(timer_gettime, F_TIMER_GET, (timer_t timer, struct itimerspec *value), (timer, value))
WRAP_ERRNO(timer_delete, F_TIMER_DELETE, (timer_t timer), (timer))
ssize_t __real_write(int fd, const void *data, size_t length);
/**
 * @brief Intercept write for controlled test failures.
 * @param[in] fd Native output descriptor.
 * @param[in] data Readable output bytes.
 * @param[in] length Number of bytes.
 * @return Native byte count or injected zero/-1 result.
 */
ssize_t __wrap_write(int fd, const void *data, size_t length)
{
	int code;
	if (selected == F_WRITE_PARTIAL) {
		selected = F_WRITE;
		return __real_write(fd, data, length > 2 ? 2 : length);
	}
	if (consume(F_WRITE_ZERO)) {
		return 0;
	}
	code = consume(F_WRITE);
	if (code) {
		errno = code;
		return -1;
	}
	return __real_write(fd, data, length);
}
