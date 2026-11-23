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

# 系统与机制移植

职责分层见 [架构](ARCHITECTURE.md)；实际 API 和类型见 [OSI 参考](reference/OSI_API.md)。
当前 Linux/NuttX 共用 `osdep/posix` 的必要分支，首期验证来自 Linux；不伪造目标宏
把主机执行当成 NuttX 成功。macOS 与其他 RTOS 仍是后续工作。

## OSI 与 base

OSI 提供 thread、mutex/sem/cond、单调时间、sleep、原生 timer、系统分配和输出。
接口使用 `osi_*`，类型为 `os_*_t`，按功能放在 `include/embt/osi`；
POSIX storage 随 thread/sync/timer 功能头提供。调用方初始化资源、拥有存储并安排安全回收。

base 定义 queue/work/slab/atomic 等机制，直接组合 OSI，不恢复 `k_*` 函数、kernel/device
或生成 syscall shim。base 操作头内部使用；公开对象内嵌 storage 与公开操作分开。
每个 queue 拥有独立 mutex，init/deinit 时创建/销毁，不使用隐藏 runtime 或全局 queue 锁。
类型与操作在 include/embt/base 的对应功能头，不建立单独 types 目录；通用工具也由 base 提供。

## 时间与线程

所有等待、sleep 和 work 相对 timeout 使用 ms：`-1` forever、`0` 不等待、正数有限等待。
有限等待按单调绝对 deadline 扣减，不在 EINTR、重试或唤醒后重新获得完整 timeout。
微秒测量使用 `osi_time_get_us`；保留 Bluetooth 协议规定的 wire unit，不能把它们机械改为 ms。
代码不恢复 tick、频率配置或调度 tick 换算。

线程 create 即可以执行，先初始化所有被引用的资源；POSIX start 不负责延迟/恢复执行。
cancel 只提交请求，仍需 join；join 不带 timeout。业务需要正常停止、释放自己的资源，
不能依靠 native cancellation 保证机制对象安全。

线程与身份只使用 `os_thread_t`。self 返回借用句柄，不产生新的 join 责任；复制句柄
只用于有效期内的观察/比较。用 `osi_thread_equal` 比较，不能直接比较 native 表示或
把 0 当作通用无效值。join 和身份失效之后不再使用旧句柄；存储指针不是线程身份。

Linux 接收原 priority/stack_size 请求，但使用 pthread 默认属性。NuttX 的 `.c` 分支
按 `CONFIG_EMBT_THREAD_PRIORITY_BASE - request`（默认基准 150）转换优先级，验证范围并
应用栈请求。保留原 Kconfig 默认与可覆盖值，不通过新的平台 choice 把它们改成统一常数。
逻辑优先级断言不等于 Linux 实时调度保证；不能假设 cooperative priority 自动提供互斥。

## 同步、timer 与 work

mutex 支持明确的 normal/recursive 类型；cond wait 的持锁与生命周期见 API 参考。
semaphore 是有上限的计数对象，give 串行维护上限；reset 只排空静止对象，不更换
limit/initial count，也不唤醒正在等待的线程。已经初始化的原生对象不能直接重复 init。
重建前必须没有等待者、回调或引用，deinit 后再 init；不要清零活动对象。

OSI 原生 timer 使用一次性 monotonic 调度和 native notification。callback 可重叠；
stop/delete 不等待已派发 callback，不能因此释放、复用或清零 timer/context。
测试采用长期存储；需要可回收逻辑超时的机制必须有自己的完成屏障。

base/work 按 Zephyr 状态机维护 cancel/flush/drain/plug 和重提交，worker 使用 semaphore，
共享 timeout dispatcher 投递 delayed work。停止、join、deinit 前须阻止外部重提交并
满足 work 的完成契约；flush、drain 和取消不是同一操作。

## 内存、atomic 和编译器

`osi_malloc/calloc/aligned_alloc` 是系统分配源，失败返回 NULL，无申请 timeout；
net_buf 的动态 payload 按约定失败断言。不要把这个上层策略写成所有 OSI 分配都 assert。
net_buf pool/slab 管理固定资源，不用自建 heap。借用、引用转移和最后一次释放由模块明确管理。

atomic 仅在 `include/embt/base/atomic.h`，保持 native long/pointer 宽度、对齐、返回旧值、
strong CAS 和 sequentially consistent 排序，见 [atomic 规范](ATOMIC_DESIGN.md)。
公开字段保持必要存储 ABI；外部通过业务 API 读取状态，不能读取内部原子字段。

必要编译器属性、非对齐工具和通用宏使用当前的最小真实 provider；不恢复 Zephyr toolchain
选择树。无当前消费者且缺少实现的 div64 头已删除；仍不能据 64 位编译通过推断目标 ABI。

## 日志、断言与平台 I/O

`base/log.h` 保留 LOG_*、hexdump、限频和 buffer helper。
根 `Kconfig.log` 中 `EMBT_LOG` 是总开关，模块 level `-1` 继承、`0..4` 显式选择。
编译条件用 `BT_LOG_LEVEL_ENABLED(module, level)`，运行 admission 用 `BT_LOG_ENABLED(level)`。
运行过滤只能限制已编译级别。LOG_MODULE_REGISTER/DECLARE 不创建 mutable registry 或 startup section。

日志格式化不分配堆，出口为无 level 的 `osi_vprint(fmt, args)`：Linux stderr、NuttX syslog。
`osi_console_write` 另用于原始字节输出。assert/fatal 独立于日志级别；
蓝牙 common/assert.h 保留，base/assert.h 做内部适配，公共工具使用自己的标准 C 检查。
不恢复 printk、diagnostic 包装或旧 Zephyr logging backend。

HCI transport 直接调用平台 I/O；它不因此变成 OSI 的协议对象。
user channel、TCP/Unix 拆包、buffer ownership 和错误路径要各自验证。
音频 PCM 后端归 drivers，不把 Pulse/NuttX native buffer 类型扩散到协议 core。

## 新平台验收

先实现真实 storage 与接口，再验证 [OSI 契约](OSI_CONTRACT_TESTS.md)、目标 ABI/时间、
资源回收及所需 transport/third-party provider。平台专属集成测试需有独立 label/环境。
当前的公开布局、POSIX 函数和 GNU linker wrap 不自动具备任意目标可用性。
明确缺少的能力和失败，不用空宏、成功 stub 或全套私有 include 绕过检查。
