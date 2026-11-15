<!--
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
-->

# 内部 OSI API 参考

本页以当前 `include/embt/osi` 声明为准；下列清单从实际头文件整理。
系统操作仍遵守内部 API 合同；共享头文件目录不宣称所有操作是应用接口。
类型与声明位于对应的 thread/sync/timer 功能头，backend helper 留在实现目录。
共同移植原则见 [PORTING](../PORTING.md)，测试合同见 [OSI_CONTRACT_TESTS](../OSI_CONTRACT_TESTS.md)。

## 对象与共同合同

`os_mutex_t/os_cond_t` 使用原生 POSIX 存储，`os_sem_t` 加 give 锁与 limit；
thread/timer 类型使用句柄及必要 context，不增加与上层 work/queue 重复的状态机。
不能复制或清零活跃同步对象和 timer/context，init/create 和 deinit/join 由拥有者串行管理。

线程与线程身份统一使用 `os_thread_t`，POSIX 后端直接别名到 `pthread_t`，不再提供
`os_tid_t` 或单字段包装。create 产生一次 join 责任；self 返回借用的同类型身份。
句柄可以复制用于比较，但复制不转移或增加 join 责任，也不创建新线程。
比较使用 `osi_thread_equal`，不能假定 native 句柄是整数/指针或以 0 表示无效；
join/原生身份失效后不能继续使用该句柄。

状态接口返回 0 或负错误；分配接口返回指针/NULL，时间等输出参数按各自合同处理。
timeout 为 ms，-1 forever、0 不等待、正值有限等待；timer start 只接受正值。
线程 create 即运行，start 为 POSIX no-op；cancel 后仍 join，join 不带 timeout。
Linux 保留默认 pthread 属性，NuttX 使用原 priority/stack 请求及基准减法。

mutex/cond wait 要求正确持锁和 predicate loop；cond 通知不积累 credit。
sem reset 仅用于没有并发用户/等待者的对象，排空 count，不重设 limit。
timer stop/delete 不等待已派发 callback，storage 和 context 必须继续存活。
OSI 分配失败返回 NULL，net_buf 的 fail-fast 策略由上层实现。
`osi_vprint` 无 level，`osi_console_write` 是原始字节输出，fatal 不返回。

## sync

完整前提、单位、返回与生命周期见 [实际头文件](../../include/embt/osi/sync.h)。

```c
int osi_mutex_init(os_mutex_t *mutex, os_mutex_kind_t kind);
int osi_mutex_deinit(os_mutex_t *mutex);
int osi_mutex_lock(os_mutex_t *mutex, os_timeout_t timeout_ms);
int osi_mutex_unlock(os_mutex_t *mutex);
int osi_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t limit);
int osi_sem_deinit(os_sem_t *sem);
int osi_sem_take(os_sem_t *sem, os_timeout_t timeout_ms);
int osi_sem_give(os_sem_t *sem);
int osi_sem_count_get(os_sem_t *sem, uint32_t *count);
int osi_sem_reset(os_sem_t *sem);
int osi_cond_init(os_cond_t *cond);
int osi_cond_deinit(os_cond_t *cond);
int osi_cond_wait(os_cond_t *cond, os_mutex_t *mutex, os_timeout_t timeout_ms);
int osi_cond_signal(os_cond_t *cond);
int osi_cond_broadcast(os_cond_t *cond);
```

## thread

完整前提、单位、返回与生命周期见 [实际头文件](../../include/embt/osi/thread.h)。

```c
int osi_thread_create(os_thread_t *thread, void (*start_routine)(void *), void *argument, const char *name, int priority, size_t stack_size);
int osi_thread_start(os_thread_t *thread);
int osi_thread_cancel(os_thread_t *thread);
int osi_thread_join(os_thread_t *thread);
os_thread_t osi_thread_self(void);
bool osi_thread_is_current(const os_thread_t *thread);
int osi_thread_yield(void);
int osi_thread_name_set(os_thread_t *thread, const char *name);
bool osi_thread_equal(os_thread_t first, os_thread_t second);
```

## timer

完整前提、单位、返回与生命周期见 [实际头文件](../../include/embt/osi/timer.h)。

```c
int osi_timer_create(os_timer_t *timer, os_timer_cb_t callback, void *argument);
int osi_timer_start(os_timer_t *timer, os_timeout_t timeout_ms);
int osi_timer_stop(os_timer_t *timer);
int osi_timer_delete(os_timer_t *timer);
int osi_timer_remaining_ms(os_timer_t *timer, uint64_t *milliseconds);
```

## time

完整前提、单位、返回与生命周期见 [实际头文件](../../include/embt/osi/time.h)。

```c
int osi_time_get_ms(uint64_t *milliseconds);
int osi_time_get_us(uint64_t *microseconds);
int osi_sleep_ms(os_timeout_t milliseconds);
int osi_timeout_from_us(int64_t microseconds, os_timeout_t *milliseconds);
int osi_timeout_from_seconds(int64_t seconds, os_timeout_t *milliseconds);
```

## memory

完整前提、单位、返回与生命周期见 [实际头文件](../../include/embt/osi/memory.h)。

```c
void *osi_malloc(size_t size);
void *osi_calloc(size_t count, size_t size);
void *osi_aligned_alloc(size_t alignment, size_t size);
void osi_free(void *pointer);
```

## io

完整前提、单位、返回与生命周期见 [实际头文件](../../include/embt/osi/io.h)。

```c
void osi_vprint(const char *fmt, va_list args);
int osi_console_write(const void *data, size_t length, size_t *written);
OS_NORETURN void osi_fatal(const char *message);
```
