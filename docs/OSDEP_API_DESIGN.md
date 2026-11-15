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

# OSI API 与对象类型设计（精简版）

状态：本地 review 草案，2026-11-15。按用户确认的轻量方向更新内部声明和 POSIX 类型，Doxygen 注释已统一。
函数后端和 CMake/CTest 已实现。四个 demo 已删除，test/osdep 使用 cmocka 验证全部 OSI 模块。
改动未暂存、提交或推送；本机功能与 sanitizer 结果见 [测试结果](OSDEP_TEST_RESULTS.md)，不等同于协议栈运行完成。

分层见[基础机制与 OSI 设计](BASE_OSI_ARCHITECTURE.md)。OSI 先作为 POSIX 原语的薄封装，
采用 OpenBlue 的轻量类型风格；只为实际接口需求增加字段，不建立重复的对象状态机。

## 1. 设计范围

- 当前只设计 Linux/POSIX，不开展 FreeRTOS 适配。
- 函数使用 osi_，类型保留 os_*_t，常量保留 OS_*。
- 头文件放在 osdep 内部，不安装、不向应用传播 include 路径。
- mutex、cond 直接使用原生类型；thread 只保存 pthread_t。
- sem 使用原生 sem_t，只增加串行 give 所需的 mutex 和 limit。
- timer 使用原生 timer_t、callback、argument，不为每个对象建立常驻辅助线程。
- queue、pool、net_buf、work、atomic、逻辑超时与执行器仍归 base。

接口只保证有效对象上的正常操作与错误转换。调用方负责初始化顺序、生命周期、
并发使用边界和回调参数寿命；不额外承诺检测所有误用。
固定 storage 和显式 init/create 保留，不使用 constructor 或自动运行 hook。

## 2. 内部目录和引用

```text
osdep/
    include/osi/
        common.h
        types.h
        sync.h
        thread.h
        time.h
        timer.h
        memory.h
        io.h
        osi.h
    posix/include/osi/
        native_types.h
```

| 文件 | 职责 |
| --- | --- |
| [common.h](../osdep/include/osi/common.h) | timeout、线程 entry、mutex 种类和基本结果约定 |
| [types.h](../osdep/include/osi/types.h) | 引入 POSIX 类型，提供 storage 初始化宏 |
| [native_types.h](../osdep/posix/include/osi/native_types.h) | 完整 POSIX 类型定义 |
| [sync.h](../osdep/include/osi/sync.h) | mutex、sem、cond 的初始化、等待与销毁 |
| [thread.h](../osdep/include/osi/thread.h) | OpenBlue 风格 create/start/cancel/join、身份、名字和 yield |
| [timer.h](../osdep/include/osi/timer.h) | 一次性 native timer 的 create/start/stop/delete/query |
| [time.h](../osdep/include/osi/time.h) | 单调时间、有限 sleep 和受检查的单位转换 |
| [memory.h](../osdep/include/osi/memory.h) | 系统 heap 来源和配对释放 |
| [io.h](../osdep/include/osi/io.h) | 原始字节输出及 fatal |
| [osi.h](../osdep/include/osi/osi.h) | 内部聚合入口 |

使用 `<osi/sync.h>`、`<osi/thread.h>`、`<osi/timer.h>` 或 `<osi/osi.h>`。
base、Host 和驱动内部可以调用；应用不需要 OSI 头文件。
完整类型对内部源码可见，不成为公开 ABI，也不通过 void * 隐藏额外对象状态。

后续 CMake 为实现 target 私有提供 osdep/include 和 osdep/posix/include，以及 POSIX feature 定义。
原生 timer_t 需要相应 feature test macro 在系统头文件之前生效；当前检查使用 -D_GNU_SOURCE，
后续 POSIX 实现 target 使用私有定义，不在头文件中修改系统 feature 宏，不传播到应用或第三方库。
对外 Bluetooth 头文件不能要求消费者构造 OSI 对象；现有 k_* 暴露边界在接入时单独梳理。
本轮不修改导入协议头文件或原配置头引用；CMake 已按测试规划接入真实 backend 与测试。

## 3. 类型及 storage

核心类型如下，完整定义见 native_types.h：

```c
typedef pthread_mutex_t os_mutex_t;
typedef pthread_cond_t os_cond_t;
typedef pthread_t os_tid_t;

typedef struct os_sem {
    sem_t native;
    pthread_mutex_t give_lock;
    uint32_t limit;
} os_sem_t;

typedef struct os_thread {
    pthread_t native;
} os_thread_t;
```

timer 仅保留 native timer_t、callback 和 argument。
移除 owner/depth/kind/initialized、cond 的关联/waiter 状态、sem 的逻辑 count/generation、
thread 的 gate/stop/state/joining 和 timer 的线程/锁/条件变量/回调状态。

OS_OBJECT_STORAGE_INIT 只是普通 storage 初始化；零内容不能用来判断 native 对象是否有效。
不要求 deinit 或 join 后 memset 为零。初始化过的 pthread/sem 对象不能复制、移动或覆盖。
调用方知道哪些资源初始化成功，并按依赖顺序回收；失败路径释放已成功创建的资源和临时分配。

### 3.1 当前大小

使用 GCC 13.3、x86_64-linux-gnu 将 sizeof 编译为汇编常量，没有执行测量程序：

| 类型 | 上版 | 精简版 |
| --- | --- | --- |
| os_mutex_t | 64 bytes | 40 bytes，与 pthread_mutex_t 一致 |
| os_cond_t | 64 bytes | 48 bytes，与 pthread_cond_t 一致 |
| os_sem_t | 112 bytes | 80 bytes |
| os_thread_t | 120 bytes | 8 bytes，仅 pthread_t |
| os_timer_t | 136 bytes | 24 bytes |

os_timeout_t 仍为 int64_t、8 bytes；native sem_t 为 32 bytes。
数值不是跨平台固定大小，不包含系统线程栈或 OS 内部资源。
SIGEV_THREAD 通知仍可能由系统创建 callback 线程，不声称 timer 没有线程或分配成本。
记录在 /tmp/embt-osi-simple-review/sizes.s 和 sizes.json。

## 4. timeout 与结果

os_timeout_t 表示相对毫秒：NO_WAIT=0，FOREVER=-1，正值有限，低于 -1 非法。
资源暂不可用时 NO_WAIT 返回 -EAGAIN，有限等待到期返回 -ETIMEDOUT；join 不带 timeout。
timer_start 只接受正值；sleep 只接受非负有限值。

状态函数返回 0 或负 errno，分配失败返回 NULL。pthread 函数使用 -rc，
返回 -1 的 libc/syscall 使用 -errno；不能混用旧 errno。
有限等待使用单调时钟，不能默默退化为无限等待；POSIX 实现可使用相应 clocklock/clockwait API，
目标 libc 不支持时必须有真实有限实现或明确返回 -ENOTSUP。
时间转换检查溢出并返回 -EOVERFLOW；正微秒向上取整为毫秒，转换错误不改变输出。

所有接口用于线程上下文，不从异步 signal handler 调用，也不提供 ISR 版本。

## 5. 同步接口

### 5.1 mutex 与 cond

mutex 保留 init(kind)、lock(timeout)、unlock、deinit。
OS_MUTEX_NORMAL 和 OS_MUTEX_RECURSIVE 通过 pthread_mutexattr_settype 选择，不存储第二份属性。
普通 mutex 不能递归取得；unlock 必须由 owner 执行，递归 mutex 的 lock/unlock 次数匹配。
这是调用方前提，不承诺所有非法调用都返回 -EDEADLK/-EPERM。
deinit 前必须没有持锁、等待或并发用户，不能依靠返回 -EBUSY 检测全部错误生命周期。

cond 保留 init、wait(cond, mutex, timeout)、signal、broadcast、deinit。
init 设置 CLOCK_MONOTONIC。wait 使用 caller 持有的同一个普通 mutex，
原子登记等待并释放锁，正常唤醒或超时返回前重新取得锁。
允许虚假唤醒，caller 在 while 中检查 predicate；NO_WAIT 不释放 mutex，返回 -EAGAIN。
predicate 更新和通知放在同一 mutex 下。cond 没有 stored credit，也不额外跟踪 waiter。
deinit 前由 caller 确保没有等待者和并发访问。

### 5.2 semaphore

```c
int osi_sem_init(os_sem_t *sem, uint32_t initial_count, uint32_t limit);
int osi_sem_deinit(os_sem_t *sem);
int osi_sem_take(os_sem_t *sem, os_timeout_t timeout_ms);
int osi_sem_give(os_sem_t *sem);
int osi_sem_count_get(os_sem_t *sem, uint32_t *count);
int osi_sem_reset(os_sem_t *sem);
```

limit 必须为正且不超过 native SEM_VALUE_MAX；initial_count 不超过 limit，超出报错，不截断。
give_lock 只串行化多个 give 的 sem_getvalue 检查和 sem_post，达到 limit 返回 0 且不 post。
take 使用原生 semaphore，不在阻塞等待期间持有 give_lock；并发 take 只减少计数，
不会破坏上限。这样修复 OpenBlue 的并发 give 竞态，无需重新实现 semaphore 等待机制。
count_get 为 native 诊断快照，不是后续 take 的成功保证。

reset 只允许在没有 waiters 和并发 API 用户时 drain 到零；不取消正在等待的 take。
这个前提由调用方负责，wrapper 不新增 waiter/generation 检测。
deinit 同样需要无并发用户和等待者。

当前 bluetooth/ 和 lib/ 中没有发现直接 k_sem_reset 调用，但这不证明今后所有机制都不需要它。
Zephyr k_sem_reset 的原契约会中止等待者，不能直接把本版 reset 当作等价实现。
如果过渡机制确实需要该行为，应在对应机制设计中保留，不能静默丢失取消语义。

## 6. 线程：完整 API 清单

保留 OpenBlue 的线程接口分组，统一 osi_ 前缀；join 按已确认规则删除 timeout。
下面列出完整声明，避免只展示 create/join 造成接口被裁掉的误解：

```c
int osi_thread_create(os_thread_t *thread, void (*start_routine)(void *), void *argument,
                      const char *name, int priority, size_t stack_size);
int osi_thread_start(os_thread_t *thread);
int osi_thread_cancel(os_thread_t *thread);
int osi_thread_join(os_thread_t *thread);
os_tid_t osi_thread_self(void);
bool osi_thread_is_current(os_thread_t *thread);
int osi_thread_yield(void);
int osi_thread_name_set(os_thread_t *thread, const char *name);
bool osi_thread_id_equal(os_tid_t first, os_tid_t second);
```

os_thread_t 仍只有 pthread_t。恢复 start/cancel 不增加 gate、停止标记或 control 状态机。
移除之前额外设计的 os_thread_attr_t、policy enum、stack requirements 类型和查询接口。
不提供 caller stack 地址参数，stack_size 只指定系统创建的线程栈大小。

### 6.1 create 与 start

create 采用直接参数形式：

| 参数 | 当前 POSIX 草案约定 |
| --- | --- |
| start_routine、argument | 与 OpenBlue 一致；argument 活到 join 成功，routine 返回终止用户执行 |
| name | 可选诊断名称，创建期间消费/复制；不是延迟启动条件 |
| priority | 首期 POSIX 默认值为 0；非零映射尚未引入，返回 -ENOTSUP，不静默忽略 |
| stack_size | 0 使用系统默认栈，正值通过 pthread 创建属性请求；不合法尺寸明确报错 |

create 后 user entry 可以立即运行，甚至先于 create 返回。
先初始化线程会访问的 mutex、cond、sem 和业务数据，再调用 create。
start 在 POSIX 中沿用 OpenBlue 的兼容空操作，只用于保留调用序列；
它不会延迟、重新启动或恢复线程，不能把 start 之前当作用户代码尚未运行的阶段。

stack 和支持的调度请求在 pthread_create 前检查，失败清理临时参数和属性。
name 是诊断提示，创建前验证目标支持的格式/长度并复制必要数据，native wrapper 可在用户入口前设置。
线程已创建后，诊断命名失败不能被报告成“create 失败且没有线程”；需要确认命名结果的调用方
使用独立 name_set，其返回值不改变创建成功或线程回收责任。
当前 POSIX backend 已实现命名提示和独立 name_set；相关创建失败路径有故障注入测试。

### 6.2 cancel 与 join

cancel 封装 pthread_cancel，成功仅表示取消请求已提交。
默认 POSIX deferred cancellation 是否生效取决于目标线程取消状态及 cancellation point；
不会保证即时退出，也不会自动正确释放业务 mutex、buffer 或其他资源。
不能把 cancel 返回当作允许释放 argument 的依据。

join 不带 timeout，阻塞等待 native 线程完成，同时回收其 joinable 资源。
cancel 后仍要 join；caller 对有效线程只 join 一次，不 self-join 或并发重复 join。
合作停止 predicate 仍可由任务/workqueue 自己管理，cancel API 的恢复不强制业务改用取消。
不增加 request_stop、stop_requested、destroy 或线程状态枚举。

entry trampoline 如需临时参数，成功转交和 pthread_create 失败都必须释放；
wrapper 在处理取消前不能留下失去所有者的启动参数。
用户线程若跨取消点持有资源，需要对应 cleanup 或受控退出机制。

### 6.3 身份、名字和 yield

self 返回 pthread_t，is_current 和 id_equal 使用 pthread_equal，不假定整数或用 == 比较。
name_set 显式报告 pthread 命名结果；yield 封装本机调度让出操作。
这些操作使用有效线程身份，join 后不继续访问旧 handle；生命周期由调用方管理。

首期 userchan RX 创建使用 K_NO_WAIT。Mesh 的 K_FOREVER/create 后 start 调用不在首期构建。
保留其源文件；将来支持 Mesh 时仍要实现对应的延迟启动语义，POSIX start 空操作不是等价适配。

## 7. 原生一次性 timer

```c
int osi_timer_create(os_timer_t *timer, os_timer_cb_t callback, void *argument);
int osi_timer_start(os_timer_t *timer, os_timeout_t timeout_ms);
int osi_timer_stop(os_timer_t *timer);
int osi_timer_delete(os_timer_t *timer);
int osi_timer_remaining_ms(os_timer_t *timer, uint64_t *milliseconds);
```

create 使用 CLOCK_MONOTONIC 与 SIGEV_THREAD，建立资源而不自动启动。
start 接受正相对毫秒，0/FOREVER/其他负值非法；重新 start 替换系统计划，
不能撤回已经派发的通知。剩余时间通过 timer_gettime 查询，正值向上取整为毫秒，
未启动/停止后的计划为 0；查询错误独立返回，0 不代表 callback 已结束。

stop 解除系统计时计划；delete 删除 native timer 资源。
两者都不保证已派发 callback 结束，不提供 callback drain、self-delete 检测或 deleting 状态。
调用方串行化 timer API 生命周期，callback 只发布通知，不调用 timer API。
原生 callback 线程可能重叠，包括同一个 timer 的多次重设通知；共享数据必须真实同步。

### 7.1 存储寿命

timer_delete 返回后，可能仍有 callback 访问 timer->callback/argument。
因此不能据此清零、复用或释放 timer、argument 及其引用的数据。
当前使用方式限定为长期存活的 timer 和通知数据。cmocka 用例将采用进程寿命的静态 storage，
删除后保持 callback 字段和数据有效，每个 timer 用例运行在独立进程。
不能在 native delete 后让仍可能被访问的栈局部 timer/context 离开作用域。

该原生设计尚不能支持“动态对象 delete 后立即释放”的要求。
如果后续 base 的对象回收需要此保证，再评估共享通知执行器及注销完成机制；
本轮不设计共享 manager，也不为每 timer 创建常驻线程。
协议处理与逻辑 timer/work 状态由 base 执行机制安排，callback 只做简短通知。

## 8. 时间、内存和输出

保持 osi_time_get_ms、osi_sleep_ms、受检查的 us/seconds 转换。
sleep 中断后继续剩余时间；0 立即返回，yield 有独立接口。

osi_malloc/calloc/aligned_alloc/free 只提供系统 heap 来源，不管理 net_buf/pool，
不替换第三方库配置；零大小、overflow 或分配失败返回 NULL，free(NULL) 允许。
其他指针来源与释放必须配对。aligned_alloc 要求合法 power-of-two alignment，
size 不必是 alignment 的倍数。

osi_console_write 只输出字节，通过 written 报告 partial write；允许阻塞，不保证消息不交错。
格式化、等级和消息同步由 base/debug 处理。osi_fatal 不返回；不引入旧 logger 兼容宏。

## 9. 测试要求与当前验证状态

测试代码在根目录 test/osdep，原 demo 已删除。全部 39 个 API 已有 POSIX 实现，
29 个用例覆盖正常、边界、并发和失败回滚；SYSTEM、SOURCE 及 ASan/UBSan 组合均通过。

接入与用例矩阵见 [OSDEP_TEST_PLAN.md](OSDEP_TEST_PLAN.md)，实际执行证据、TSan 启动限制和
未运行的远端 CI 状态见 [OSDEP_TEST_RESULTS.md](OSDEP_TEST_RESULTS.md)。
测试直接复用真实 backend object，不依赖 Bluetooth Host 编译完成，也不改变原生轻量类型。

当前 POSIX console 使用 stderr、重试 EINTR 并报告 partial write；NULL+0 成功。
fatal 输出可选诊断后 abort。mutex kind/sem 数值边界返回 -EINVAL，thread 名字超限 -ERANGE，
非零 priority -ENOTSUP。timer 的 callback 寿命和 native delete 边界保持已有契约。
