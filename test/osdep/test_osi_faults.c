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
 * @brief Failure conversion, rollback, interruption and partial output tests.
 */

#include "support/test_support.h"
#include "support/faults.h"
#include <signal.h>
static void empty_entry(void *argument)
{
	(void)argument;
}
/**
 * @brief Verify fault thread rollback behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_fault_thread_rollback(void **state)
{
	os_thread_t thread;
	unsigned int destroyed;
	(void)state;
	test_fault_arm(F_ATTR_INIT, EAGAIN);
	errno = 0;
	assert_int_equal(osi_thread_create(&thread, empty_entry, NULL, NULL, 0, 0), -EAGAIN);
	destroyed = atomic_load(&test_attr_destroyed);
	test_fault_arm(F_MALLOC, ENOMEM);
	assert_int_equal(osi_thread_create(&thread, empty_entry, NULL, NULL, 0, 0), -ENOMEM);
	assert_int_equal(atomic_load(&test_attr_destroyed), destroyed + 1);
	test_fault_arm(F_CREATE, EAGAIN);
	errno = 0;
	assert_int_equal(osi_thread_create(&thread, empty_entry, NULL, NULL, 0, 0), -EAGAIN);
	assert_int_equal(atomic_load(&test_live_allocations), 0);
	assert_int_equal(osi_thread_create(&thread, empty_entry, NULL, NULL, 0, 0), 0);
	assert_int_equal(osi_thread_join(&thread), 0);
	assert_int_equal(atomic_load(&test_live_allocations), 0);
}
/**
 * @brief Verify fault sync rollback behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_fault_sync_rollback(void **state)
{
	os_mutex_t mutex;
	os_cond_t cond;
	os_sem_t sem;
	const enum test_fault mutex_faults[] = {F_MUTEX_ATTR_INIT, F_MUTEX_ATTR_TYPE, F_MUTEX_INIT};
	const enum test_fault cond_faults[] = {F_COND_ATTR_INIT, F_COND_ATTR_CLOCK, F_COND_INIT};
	(void)state;
	for (size_t i = 0; i < TEST_ARRAY_SIZE(mutex_faults); i++) {
		unsigned int before = atomic_load(&test_mutexattr_destroyed);
		test_fault_arm(mutex_faults[i], EAGAIN);
		errno = 0;
		assert_int_equal(osi_mutex_init(&mutex, OS_MUTEX_NORMAL), -EAGAIN);
		assert_int_equal(atomic_load(&test_mutexattr_destroyed), before + (i != 0));
	}
	for (size_t i = 0; i < TEST_ARRAY_SIZE(cond_faults); i++) {
		unsigned int before = atomic_load(&test_condattr_destroyed);
		test_fault_arm(cond_faults[i], EAGAIN);
		errno = 0;
		assert_int_equal(osi_cond_init(&cond), -EAGAIN);
		assert_int_equal(atomic_load(&test_condattr_destroyed), before + (i != 0));
	}
	unsigned int before = atomic_load(&test_mutex_destroyed);
	test_fault_arm(F_SEM_INIT, EIO);
	assert_int_equal(osi_sem_init(&sem, 0, 1), -EIO);
	assert_int_equal(atomic_load(&test_mutex_destroyed), before + 1);
	assert_int_equal(osi_sem_init(&sem, 0, 1), 0);
	test_fault_arm(F_SEM_VALUE, EIO);
	assert_int_equal(osi_sem_give(&sem), -EIO);
	test_fault_arm(F_SEM_POST, EIO);
	assert_int_equal(osi_sem_give(&sem), -EIO);
	assert_int_equal(osi_sem_give(&sem), 0);
	assert_int_equal(osi_sem_deinit(&sem), 0);
}
/**
 * @brief Verify fault time memory behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_fault_time_memory(void **state)
{
	uint64_t timestamp = 17;
	uint64_t began;
	(void)state;
	test_fault_arm(F_CLOCK, EIO);
	assert_int_equal(osi_time_get_ms(&timestamp), -EIO);
	test_fault_arm(F_CLOCK, EIO);
	assert_int_equal(osi_sleep_ms(20), -EIO);
	test_fault_arm(F_CLOCK_LIMIT, 1);
	assert_int_equal(osi_sleep_ms(2), -EOVERFLOW);
	test_fault_arm(F_CLOCK_LIMIT, 1);
	assert_int_equal(osi_time_get_ms(&timestamp), -EOVERFLOW);
	test_fault_arm(F_SLEEP, EIO);
	errno = 0;
	assert_int_equal(osi_sleep_ms(20), -EIO);
	began = test_now_ms();
	test_fault_arm(F_SLEEP, EINTR);
	assert_int_equal(osi_sleep_ms(25), 0);
	assert_true(test_now_ms() - began >= 24);
	test_fault_arm(F_MALLOC, ENOMEM);
	assert_null(osi_malloc(4));
	test_fault_arm(F_CALLOC, ENOMEM);
	assert_null(osi_calloc(1, 4));
	test_fault_arm(F_ALIGN, ENOMEM);
	assert_null(osi_aligned_alloc(64, 4));
	assert_int_equal(atomic_load(&test_live_allocations), 0);
}
/**
 * @brief Verify fault wait interrupted behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_fault_wait_interrupted(void **state)
{
	os_sem_t sem;
	(void)state;
	assert_int_equal(osi_sem_init(&sem, 0, 1), 0);
	test_fault_arm(F_SEM_WAIT, EINTR);
	assert_int_equal(osi_sem_take(&sem, 25), -ETIMEDOUT);
	test_fault_arm(F_SEM_WAIT, EIO);
	assert_int_equal(osi_sem_take(&sem, 25), -EIO);
	assert_int_equal(osi_sem_deinit(&sem), 0);
}
static os_timer_t timer;
static void empty_callback(os_timer_t *source, void *argument)
{
	(void)source;
	(void)argument;
}
/**
 * @brief Verify fault timer behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_fault_timer(void **state)
{
	uint64_t remaining = 71;
	(void)state;
	test_fault_arm(F_TIMER_CREATE, EIO);
	assert_int_equal(osi_timer_create(&timer, empty_callback, NULL), -EIO);
	assert_int_equal(osi_timer_create(&timer, empty_callback, NULL), 0);
	test_fault_arm(F_TIMER_SET, EIO);
	assert_int_equal(osi_timer_start(&timer, 60000), -EIO);
	test_fault_arm(F_TIMER_GET, EIO);
	assert_int_equal(osi_timer_remaining_ms(&timer, &remaining), -EIO);
	assert_int_equal(remaining, 71);
	test_fault_arm(F_TIMER_SET, EIO);
	assert_int_equal(osi_timer_stop(&timer), -EIO);
	test_fault_arm(F_TIMER_DELETE, EIO);
	assert_int_equal(osi_timer_delete(&timer), -EIO);
	assert_int_equal(osi_timer_delete(&timer), 0);
}
/**
 * @brief Verify fault output behavior.
 * @param[in,out] state Unused cmocka fixture state.
 */
static void test_fault_output(void **state)
{
	int descriptors[2], saved;
	char data[8];
	size_t written;
	int partial, interrupted, zero;
	(void)state;
	pipe(descriptors);
	saved = dup(STDERR_FILENO);
	dup2(descriptors[1], STDERR_FILENO);
	test_fault_arm(F_WRITE_PARTIAL, EIO);
	partial = osi_console_write("abcd", 4, &written);
	size_t partial_count = written;
	test_fault_arm(F_WRITE, EINTR);
	interrupted = osi_console_write("ef", 2, &written);
	test_fault_arm(F_WRITE_ZERO, EIO);
	zero = osi_console_write("gh", 2, &written);
	dup2(saved, STDERR_FILENO);
	close(saved);
	close(descriptors[1]);
	ssize_t count = read(descriptors[0], data, sizeof(data));
	close(descriptors[0]);
	assert_int_equal(partial, -EIO);
	assert_int_equal(partial_count, 2);
	assert_int_equal(interrupted, 0);
	assert_int_equal(zero, -EIO);
	assert_int_equal(written, 0);
	assert_int_equal(count, 4);
	assert_memory_equal(data, "abef", 4);
}
/**
 * @brief Run one registered case in its own process.
 * @param[in] argc Argument count.
 * @param[in] argv Arguments containing the exact case name.
 * @return Test result or a selection error.
 */
int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_fault_thread_rollback),
		cmocka_unit_test(test_fault_sync_rollback),
		cmocka_unit_test(test_fault_time_memory),
		cmocka_unit_test(test_fault_wait_interrupted),
		cmocka_unit_test(test_fault_timer),
		cmocka_unit_test(test_fault_output),
	};
	return test_run(argc, argv, tests, TEST_ARRAY_SIZE(tests));
}
