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
 * @brief POSIX thread creation, cancellation, join and identity operations.
 */

#include <embt/osi/thread.h>
#include "thread_config.h"
#include <stdlib.h>
#include <string.h>
#include <sched.h>

struct start_context {
	os_thread_entry_t routine;
	void *argument;
	char name[16];
};

/**
 * @brief Release startup storage before entering cancellable user code.
 * @param[in] argument Allocated start_context transferred by pthread_create.
 * @return NULL after the user routine returns.
 */
static void *thread_trampoline(void *argument)
{
	struct start_context context = *(struct start_context *)argument;

	free(argument);
	if (context.name[0]) {
		/* A diagnostic naming failure does not invalidate successful creation. */
		pthread_setname_np(pthread_self(), context.name);
	}
	context.routine(context.argument);
	return NULL;
}

/**
 * @brief Apply the platform policy inside the shared POSIX creation path.
 * @param[in,out] attributes Initialized native creation attributes.
 * @param[in] priority Original caller priority numbering.
 * @param[in] stack_size Original caller byte request; zero retains native default.
 * @return Zero on success or a negative native/validation error.
 */
static int configure_thread_attributes(pthread_attr_t *attributes, int priority, size_t stack_size)
{
#if defined(__NuttX__)
	struct sched_param scheduling = {0};
	int policy, minimum, maximum, rc;

	rc = pthread_attr_getschedpolicy(attributes, &policy);
	if (rc) {
		return -rc;
	}
	minimum = sched_get_priority_min(policy);
	if (minimum < 0) {
		return -errno;
	}
	maximum = sched_get_priority_max(policy);
	if (maximum < 0) {
		return -errno;
	}
	rc = osi_thread_priority_map(priority, minimum, maximum, &scheduling.sched_priority);
	if (rc) {
		return rc;
	}
	if (stack_size) {
		rc = pthread_attr_setstacksize(attributes, stack_size);
		if (rc) {
			return -rc;
		}
	}
	rc = pthread_attr_setschedparam(attributes, &scheduling);
	if (!rc) {
		rc = pthread_attr_setinheritsched(attributes, PTHREAD_EXPLICIT_SCHED);
	}
	return -rc;
#else
	/* Linux deliberately retains all default attributes. */
	(void)attributes;
	(void)priority;
	(void)stack_size;
	return 0;
#endif
}

int osi_thread_create(os_thread_t *thread, void (*start_routine)(void *), void *argument,
		      const char *name, int priority, size_t stack_size)
{
	pthread_attr_t attr;
	struct start_context *context;
	int rc;

	if (!thread || !start_routine) {
		return -EINVAL;
	}
	if (name && strlen(name) >= sizeof(context->name)) {
		return -ERANGE;
	}
	rc = pthread_attr_init(&attr);
	if (rc != 0) {
		return -rc;
	}
	rc = configure_thread_attributes(&attr, priority, stack_size);
	if (rc) {
		pthread_attr_destroy(&attr);
		return rc;
	}
	context = malloc(sizeof(*context));
	if (!context) {
		pthread_attr_destroy(&attr);
		return -ENOMEM;
	}
	context->routine = start_routine;
	context->argument = argument;
	context->name[0] = '\0';
	if (name) {
		memcpy(context->name, name, strlen(name) + 1);
	}
	rc = pthread_create(thread, &attr, thread_trampoline, context);
	pthread_attr_destroy(&attr);
	if (rc != 0) {
		free(context);
	}
	return -rc;
}

int osi_thread_start(os_thread_t *thread)
{
	/* POSIX creation already starts execution; there is no suspended state. */
	return thread ? 0 : -EINVAL;
}

int osi_thread_cancel(os_thread_t *thread)
{
	return thread ? -pthread_cancel(*thread) : -EINVAL;
}

int osi_thread_join(os_thread_t *thread)
{
	if (!thread) {
		return -EINVAL;
	}
	if (pthread_equal(*thread, pthread_self())) {
		return -EDEADLK;
	}
	return -pthread_join(*thread, NULL);
}

os_thread_t osi_thread_self(void)
{
	return pthread_self();
}

bool osi_thread_is_current(const os_thread_t *thread)
{
	return thread && pthread_equal(*thread, pthread_self());
}

bool osi_thread_equal(os_thread_t first, os_thread_t second)
{
	return pthread_equal(first, second) != 0;
}

int osi_thread_yield(void)
{
	return sched_yield() == 0 ? 0 : -errno;
}

int osi_thread_name_set(os_thread_t *thread, const char *name)
{
	return thread && name ? -pthread_setname_np(*thread, name) : -EINVAL;
}
