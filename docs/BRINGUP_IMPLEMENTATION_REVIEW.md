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

# Bluetooth 4.2 LE + BR 编译适配 Review

状态：本地实现，未暂存、未提交、未推送。以当前工作分支及本地 OSI 实现为基础。
本轮完成的是约定功能组合的源码编译、整库链接与机制/传输测试；无线协议栈运行基线尚未验收。
本文替代前期调研中的“待实施/首个编译阻塞”状态描述，调研中的旧日志只作为历史证据。

## 1. 验收范围与入口

- Linux x86-64 / POSIX，GCC 13.3，CMake + Ninja + 单一 Kconfig 驱动。
- LE legacy advertising/scanning、Central/Peripheral、ACL、ATT/GATT server + client。
- LE SMP Legacy Pairing + Secure Connections；保留 AES/CMAC、P-256/ECDH、随机数和原安全策略。
- BR/EDR 的连接、SSP、L2CAP、SDP、SCO 连接控制及 LE/BR 共享代码。
- Mesh、LE Audio、ISO、CS 不启用；这些协议源码继续保留。
- RFCOMM、A2DP/AVRCP、HFP 等独立可选 profile 不在本次基础配置的验收矩阵中。
- SCO 音频 payload 管线没有新增；保留的是原 Host 的 SCO 连接控制能力。

普通构建现在同时检查静态库和完整链接：

```sh
./mkbt.sh build --build-dir build/bringup
./mkbt.sh test-port
./mkbt.sh test-port --build-dir build/bringup-asan --cmake-arg -DEMBT_OSI_SANITIZER=ADDRESS
./mkbt.sh build --build-dir build/bringup-source --fragment config/crypto_source.conf
```

新 build 目录采用 `config/posix_host_defconfig`。已有目录包含旧 `CONFIG_BT_USERCHAN` 时，
先运行 `./mkbt.sh defconfig --build-dir <目录>`；自定义配置应将该符号改为 `CONFIG_EMBT_HCI_SOCKET`。
带有 SOURCE 片段的目录重置时，应继续传 `--fragment config/crypto_source.conf`。
`defconfig` 会重置配置，不能在未备份用户自定义配置时随意用于其他工作目录。

## 2. 分类改动

| 分类 | 主要文件 | 实际改动 |
| --- | --- | --- |
| 内部 atomic | `base/include/base/bt_atomic.h`、Host 调用点、公开协议结构 | 单一 GCC/Clang builtin 实现；`bt_atomic_*` / `BT_ATOMIC_*`；移除原公开 `sys/atomic*.h`；保留 long/pointer 宽度、强 CAS、旧值返回和 SEQ_CST |
| 机制与存储边界 | `base/include/base/base.h`、`include/embt/storage.h` | base 操作 API 私有；公开协议结构需要的队列/work/timeout 存储不携带 pthread/OSI 类型或 atomic API |
| 队列与兼容 semaphore | `base/core.c` | intrusive FIFO/LIFO、删除/窥视、单调 deadline、单个等待者取消；每个等待者有独立唤醒对象；semaphore 有计数上限及真实 reset/cancel 行为 |
| 固定块池与 arena | `base/core.c`、ATT/BR L2CAP 初始化 | fixed slab 显式初始化；可变尺寸 arena 在固定 backing storage 内分割、对齐、合并与等待，无 system heap fallback |
| net_buf | `include/embt/net_buf.h`、`lib/net_buf/buf.c`、Host buffer 用户 | 保留 net_buf API 和数据/fragment/ref 机制；`buf->pool` 直接保存所属 pool；去掉 pool ID、全局段表查找；固定/可变/heap 路径均有实际实现 |
| work / delayed work | `base/work.c` | 普通工作队列、pending/running/canceling、提交/重调度/取消/flush；延迟由所属 worker 的单调 deadline 列表驱动；无逐对象 native timer |
| Host 同步与生命周期 | `hci_core.c/.h`、`conn.c/conn_internal.h`、`scan.c`、`smp.c` | 显式启动、失败回滚；Host 递归锁及线程本地持锁检查；TX completion list 使用同一把锁；scan reset 不覆盖已经初始化的 pthread mutex |
| 注册与初始化 | `gatt.c`、`l2cap.c`、`att.c`、`smp.c`、Classic L2CAP/SDP/SCO | 基础服务/固定信道使用真实显式注册；连接/SCO 回调使用原动态列表；不再依赖本次构建里的 iterable linker section 或 SYS_INIT |
| HCI transport | `include/embt/bluetooth/transport.h`、`drivers/bluetooth.h`、`drivers/bluetooth/hci/socket*` | 移除 device/devicetree 绑定；显式 transport descriptor；TCP、Unix stream、Linux HCI user 三种 endpoint；共用所有权及关闭规则 |
| 头文件和旧日志 | 实际构建依赖闭包、`common/hci_lockstep.h`、配置生成器 | 真正的 utility 引用改成 embt；删旧 kernel/device/syscall/logger 引用；显式 autoconf include 改名；配置转发头退役；lockstep 移到内部 common |
| assertion / random | `base/diagnostic.c`、`random_psa.c` | 保留断言及 fatal，提供真实格式化输出；随机数继续由 PSA 提供，失败不填充伪随机值；LOG_* 调用由内部 base/log.h 适配 |
| 构建与验证 | 根 CMake、`embt_library.cmake`、`embt_testing.cmake`、`mkbt.sh`、`test/compile`、`test/bringup` | `embt_link_check` 用 whole-archive 验证全部对象的未定义符号；test-port 覆盖整栈依赖；sanitizer 仅注入项目 target |

## 3. base 的实现选择与边界

### 3.1 queue / semaphore / pool

队列对象只有链表存储。线程等待时，在栈上创建一个带 OSI semaphore 的等待节点，
在共享 metadata mutex 下挂入等待列表。put 优先把对象直接交给最早的等待者。
取消只摘除当前等待者并明确唤醒，未来 get 不受影响；无等待者时取消不会遗留状态。
超时使用操作开始时计算的单调 deadline。生产者唤醒与等待节点的释放在同一把锁下交接。

保留的 `k_sem_*` 是 Host 过渡机制，OSI 原生 `osi_sem_*` 不变。
它用 count/limit 和等待者列表表示逻辑 semaphore，解决原 API reset 会取消 waiter、
而原生 OSI reset 只允许无并发 drain 的语义差异；同时避免临时 HCI command semaphore
在 giver 尚未退出原生 give 时就销毁其锁。

队列/池的短 metadata 操作当前共享一把 mutex；这减少每个对象的 native 资源及销毁要求，
但存在跨队列竞争。未做吞吐/实时性基准，不宣称比 OpenBlue 在所有性能指标上更快。
机制等待不支持 `pthread_cancel` 式强制退出；拥有者通过取消等待、停止任务和 join 结束线程。

### 3.2 work 与延迟任务

每个启动的 work queue 有一个 OSI worker、condition variable、pending list 和有序 deadline list。
执行 handler 时不持内部 work lock。一个 work 再提交时仍归属当前执行队列，避免同一 handler 并行执行。
异步 cancel 返回仍在运行/取消中的状态并阻止当前 handler 再提交；同步 cancel 等待当前 callback 结束。
flush 等待调用时观察到的那次提交，不被未来重复提交无限延长。

延迟 work 不调用 `osi_timer_create`，由 worker 的 condition wait 等到最近 deadline。
因此同步取消可以真正摘除计时引用；不存在 native timer 已排队的回调晚到后访问已释放 work 的路径。
普通 handler 可以在没有再次提交时释放自己的 container，worker 返回后不再访问该 work。
有序列表插入为 O(n)，首期不引入 timer wheel 或复杂调度框架。

队列启动可以返回错误并释放部分资源；stop 唤醒、取消 pending/timed 项并 join worker。
调用者必须先阻止外部提交和同步 waiter；stop 不能从所属 worker 调用。

### 3.3 临界区与线程

`k_sched_lock` 在本端是参与调用者之间的递归 mutex，不是“禁止其他 POSIX 线程调度”。
实际 TX completion producer/consumer 已改为共同的 `conn->tx_complete_lock`。
Host lock 的所有权断言使用线程本地深度，避免读取 pthread 的内部布局。

本轮 worker 使用普通 POSIX 调度和默认 pthread stack；原 Zephyr cooperative priority / 小栈预算
不直接等价映射为 POSIX 调度与资源预算。它们的运行时性能与完整 Host 并发行为仍需后续 bring-up 验证。
没有把协议栈改成单线程，也没有将 HCI command 同步流程批量改为异步。

## 4. net_buf 与公开接口

`pool_id` 已从 `struct net_buf` 的实际字段移除，新增 `struct net_buf_pool *pool`。
引用计数继续使用原来的 long-sized 原子槽，原 pool_id 字节变为保留位；pool 指针独立保存。
当前 x86-64 下 sizeof(struct net_buf) 为 56 字节，ref_word 和 pool 指针各 8 字节。
新增 pool 指针增加了每个 buffer 的布局开销，所有消费者必须一起重新编译。

`net_buf_id()` 仍保留：它表示 buffer 在**所属池内部的槽号**，HCI command metadata 和固定 payload
定位仍需要这个索引；它不再代表 pool 的全局编号，也不查全局 pool table。
fixed pool backing array、variable arena、heap allocator、clone、external data、fragment 及 destroy callback
保持各自的资源模型。`net_buf_take`、`net_buf_destroy`、pool 可用量读取移入库内实现。

公开协议结构中的 atomic 字段使用 native long/pointer 存储，操作只能在内部完成。
`bt_conn_take` 同样移到库内；公开消费者编译不需要 base/include 或 osdep/include。

L2CAP 的 `ops.status` 当前保留与原 `atomic_t *` 等价的 `long *` ABI，未擅自改成值传递。
新增 `bt_l2cap_chan_status_get()` 提供安全的状态位快照，应用不要普通读写内部 status 数组。
“将回调参数直接改成 unsigned long 快照”的方案已向用户询问，尚未确认，未实施。

## 5. 显式初始化与注册

新增 `bt_stack_init()`，只准备机制和 worker，不打开控制器，也不发送 HCI command。
需要在其他 Bluetooth API 前由生命周期拥有者串行调用；`bt_enable()` 也会调用它。
先初始化 base、Host mutex、Host RX queue、long queue、TX processor，再准备连接回收 semaphore。
若启动失败，按逆序停止已启动的队列并释放初始化资源。

固定 LE/BR 信道各有按 CID 索引的 64 项表；注册校验 CID、accept 和重复冲突。
ATT、SMP、LE signaling、BR signaling 从对应 init 函数注册；ATT slabs 有实际 free-list 初始化。
GATT/GAP 基础服务在应用服务之前进入真实列表；应用服务显式注册，拒绝重复及 handle 总量溢出。

`BT_CONN_CB_DEFINE` / `BT_GATT_SERVICE_DEFINE` / `BT_L2CAP_FIXED_CHANNEL_DEFINE` 保留存储定义便利，
登记动作由明确 API 完成。应用使用这些宏时必须增加对应 register 调用，不能继续期待自动段登记。
首期禁用功能的代码仍在；这不表示它们的所有初始化路径已经完成适配。

静态数组、零初始化 metadata、常量 handler 指针继续使用 C 初始化；它们不会启动 OS 对象。
net_buf 的首个 buffer 初始化仍由分配路径完成。没有 constructor、自动启动线程或空 section 遍历宏。

## 6. HCI 三类 socket 的行为

- `bt_hci_socket_configure()` 复制配置，选择 USER、TCP 或 UNIX；没有默认悄悄连接某个 bridge。
- TCP 支持 getaddrinfo 的 IPv4/IPv6 地址解析；Unix 使用绝对路径的 stream socket。
- TCP/Unix 解析 H4 拆包和粘包；USER 按 Linux packet 边界校验完整长度，拒绝截断包。
- send 处理 EINTR、EAGAIN、短写和背压；stream 中途出错后标记链路不可再发送，避免把剩余半包当作新包重试。
- send 成功才消费调用方 buffer；失败时 buffer 仍归调用方。SIGPIPE 通过 MSG_NOSIGNAL 避免。
- RX 使用 poll + eventfd 唤醒，避免固定 1 ms 轮询。退出时先通知停止，join RX，再关闭 fd 和 native mutex。
- pool 暂时耗尽时以 50 ms 有限等待检查停止条件；不能把 close 卡在永久 buffer 分配等待中。
- close 不能从 RX callback 调用；callback 本身必须返回。测试中的关闭时限不代表任意应用 callback 的时限保证。
- connect 每个地址有 5 s 等待期限；DNS 解析本身依赖系统 resolver，未增加异步 DNS。
- 没有自动断线重连，也没有在当前测试中停用系统 Bluetooth 服务或接管 hci0。

构建开关改名为 `EMBT_HCI_SOCKET`，表示整个 socket backend。
原 `userchan.c/userchan_bottom.c` 源码保留为移植来源，但不再进入这个 backend 的构建。
底层 socket 文件只使用 Linux HCI socket ABI，不要求链接 BlueZ 的 userspace library。

## 7. 配置与生成物差异

| 项目 | 本轮处理 |
| --- | --- |
| `BT_USERCHAN` | 改为 `EMBT_HCI_SOCKET`，runtime endpoint 覆盖 USER/TCP/UNIX |
| `BT_GATT_CLIENT` | 在基础 defconfig 显式开启 |
| `BT_SMP_SC_PAIR_ONLY` | 在基础 defconfig 关闭，保留 Legacy + SC；没有启用 legacy-only |
| `BT_GATT_CACHING`、`BT_GATT_READ_MULT_VAR_LEN` | 基础 defconfig 关闭，不把 5.x 扩展作为 4.2 基线要求 |
| Mesh / Audio / ISO / CS | 基础 defconfig 显式关闭，源码保留 |
| `ASSERT_MODULE_DEFAULT_LEVEL` | 根 Kconfig 新增，默认 2，补齐真实断言 provider 的要求 |
| `EMBT_CRYPTO_SOURCE` | 新增可复用 `config/crypto_source.conf` 片段；Mbed TLS pin 仍为 3.6.5 |
| PSA / Mbed TLS 算法选项 | 继续由既有 provider 集成和能力检查处理，未重新定义一套加密配置 |
| `embt/autoconf.h` | 唯一生成头；target `-include` 与保留的显式 include 共存 |
| `zephyr/autoconf.h` | 所有显式引用已迁移，生成和追踪代码退役；旧 build 中的历史文件不再被引用 |
| LOG_* 接口 | 已在本地恢复被误删的调用与诊断 helper，接内部同步日志适配器；原 Zephyr logger 实现继续退役 |

## 8. 验证结果与限制

| 验证 | 结果 |
| --- | --- |
| 基础 LE+BR / SYSTEM crypto | 60 个项目 production compilation unit 编译，libembt.a + whole-archive consumer 链接通过 |
| SOURCE crypto | 使用已准备、锁定的 Mbed TLS 3.6.5 源码，完整编译/链接通过 |
| cmocka/CTest 普通构建 | 50/50：OSI 29、base/registry/net_buf 14、transport 7 |
| ASan + UBSan | 50/50 通过；没有屏蔽 sanitizer 错误 |
| 新增 21 项测试重复运行 | ASan/UBSan 下每项 20 次，共 420 次通过 |
| TCP / Unix | 实际本地 socket 收发、拆包/粘包、关闭、短写/背压中断通过 |
| Linux HCI USER | socket/bind 参数及 packet 收发模型通过；真实 kernel socket 创建返回 -EPERM |
| 配置头恢复 | 删除 canonical autoconf.h 后默认构建自动恢复，内容 SHA256 一致 |
| 公开头 C/C++11 | 不加入 base/osdep include 路径的 consumer 编译通过 |
| 无线运行基线 | 未验证；没有扫描、连接、配对、音频或协议一致性测试结果 |

仍有导入代码中的 unused parameter、signedness、常量范围比较等编译告警；
本轮不把大量无关代码改成“零告警”版本。实际 provider 的缺失函数和类型不匹配作为错误处理，
未通过关闭这些错误检查来完成链接。

当前机器可见 hci0，但本轮没有接管它。真实 user socket 的成功 open/close 和无线收发需要后续
具备相应权限及明确控制器占用安排的硬件测试；模型测试不能替代这个结论。
TSan 的旧环境限制仍存在，本轮不报告 TSan 全量通过。

日志位于 `/tmp/embt-final-test-port.log`、`/tmp/embt-final-asan.log`、
`/tmp/embt-final-repeat.log`、`/tmp/embt-final-source-build.log`、
`/tmp/embt-user-socket-probe.log`。JUnit、源文件 SHA256 和文件分类索引放在 build/bringup-tests：
[验证记录](../build/bringup-tests/bringup-validation.json)、[逐文件索引](../build/bringup-tests/change-index.json)。
这些是忽略的本地验证产物，不属于待提交源码。

## 9. Review 与后续提交

优先 review：公开 storage/ABI 边界、base 的等待与取消契约、work 生命周期、显式注册、transport 关闭和 buffer 所有权。
L2CAP 回调改值传递仍等用户确认；当前兼容路径已经可以完成本轮编译验收。

文档与规则仍归文档 PR；机制、Host、driver、构建变更在用户下达提交指令后再按板块整理。
当前全部新修改保持本地未暂存。没有创建 commit、push、更新 main、合入或将任何 Draft PR 标记 ready。
编译/测试通过不解除 runtime integration hold。
