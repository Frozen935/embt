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

# lib、drivers 与头文件清理

状态：本地 `refactor/lib-drivers-header-layout`，保留前序未提交的 btshell/Controller 改动。
本轮没有暂存、提交、推送或合入。原三个 Draft PR 的远端内容不因本轮本地工作改变。

## 1. 目录调整

| 原位置 | 当前位置 | 范围 |
| --- | --- | --- |
| bluetooth/subsys/crc | lib/crc | 8 个 CRC 实现及其 CMake/Kconfig，共 10 个文件 |
| bluetooth/subsys/random | lib/random | PSA 随机源及其 CMake/Kconfig，共 3 个文件 |
| bluetooth/subsys/settings | lib/settings | 保留 settings 的源文件和配置树，共 8 个文件 |
| bluetooth/drivers | drivers | 整体移动现有驱动树，共 12 个文件 |

合计移位 33 个文件。原 subsys 顶层 CMake/Kconfig 的入口并入 lib 的对应入口，
删除这两个不再需要的聚合文件；bluetooth/subsys 和 bluetooth/drivers 原目录已不存在。
驱动内部层次保留为 drivers/bluetooth/hci，没有在这次调整中再改其子目录结构。

当前主要布局：

```text
apps/btshell/
base/
bluetooth/                 Host、协议、services、保留的 Mesh/Audio 等
  host/
  common/
  crypto/
  ...
drivers/bluetooth/hci/     Linux USER、TCP、Unix socket 实现及原移植来源
include/embt/             协议/组件公开头、实际工具和必要存储
lib/
  crc/
  random/
  settings/
  net_buf/
  utils/
  uuid/
  shell/
osdep/
third_party/
test/
```

移位保留源文件内容和权限；伴随修改单独归属于构建入口或头文件依赖清理。
CRC/random 的所有文件以及驱动实现本体在本轮移位前后内容校验一致。
settings 的三个 C 文件额外移除了已经废弃的 kernel/logger include，驱动 CMake 删除了旧相对 include 路径。
原非 Controller Bluetooth 子树中的协议文件均保留，不以是否进入当前构建作为删除依据。

## 2. 构建入口

- 根 CMake 使用 add_subdirectory(lib) 和 add_subdirectory(drivers)。
- lib/CMakeLists.txt 汇集 CRC、random、settings，保留原组件目标、源选择和条件。
- lib/Kconfig source 对应组件 Kconfig；根 Kconfig source drivers/Kconfig。
- CMake 的 Kconfig 条件检查也扫描新 drivers 目录，避免遗漏驱动条件或失效的配置依赖。
- test_transport 的 socket_io.h 使用明确的 drivers/bluetooth/hci 私有 include 路径，
  不依赖旧 bluetooth include 路径，也不向普通 EmBT 消费者导出整个项目根目录。
- 公开头命名空间仍是 embt/drivers、embt/settings、embt/random 等；没有因为源码移位而再次改 API 前缀。

## 3. 头文件保留依据

清理前 include/embt 共 198 个头，删除 36 个，保留 162 个。
所有现有 Bluetooth 协议头继续保留，包括未进入首期构建的组件。
辅助头只从项目源码和保留组件的直接引用开始，递归保留实际工具依赖；
旧 kernel/device/logger/tracing 链不参与扩大保留集合。

保留的非协议头包括：

- net_buf.h、storage.h、types.h、drivers/bluetooth.h、settings/settings.h、random/random.h。
- sys 的 list、byteorder、CRC、base64、UUID、math/div64、assert、printk 和宏/时间换算工具。
- toolchain.h 及现有 GNU/LLVM 的实际编译器抽象。
- ring_buffer.h：保留的 Host monitor/USB Audio 源码有直接引用。
- iterable_sections.h：settings 及保留组件仍直接使用其定义宏；本轮没有宣称这些未启用组件的注册机制已经完成移植。

宏展开所需的 util_internal/util_listify/util_loops 等头是真实依赖，不能因为文件数量多就删掉。
旧 arch/provider 的 include 已移除；ISO 的 find_lsb_set 调用改为等价的 compiler __builtin_ffs，
保留零输入为 0、有效位置从 1 开始的语义，没有恢复 arch 头或创建空兼容文件。

## 4. 删除清单

| 类别 | 删除内容 |
| --- | --- |
| 内核与设备模型 | kernel.h、kernel_structs.h、kernel_includes.h、kernel/ 四个头、device.h、init.h、irq.h、spinlock.h、sleep.h、syscall.h、fatal.h、fatal_types.h |
| 旧 trace/log/linker metadata | tracing/ 三个头、logging/ 四个头、debug/stack.h、linker/ 两个头 |
| 旧内核辅助 provider | sys/clock.h、sys/kobject.h、sys/mem_stats.h、sys/sys_heap.h |
| 无直接组件消费者 | sys/cbprintf.h、cbprintf_cxx.h、cbprintf_enums.h、cbprintf_internal.h、mpsc_packet.h、rb.h、sflist.h |

以下为本次删除的完整相对路径：

```text
include/embt/debug/stack.h
include/embt/device.h
include/embt/fatal.h
include/embt/fatal_types.h
include/embt/init.h
include/embt/irq.h
include/embt/kernel.h
include/embt/kernel/obj_core.h
include/embt/kernel/stats.h
include/embt/kernel/thread.h
include/embt/kernel/thread_stack.h
include/embt/kernel_includes.h
include/embt/kernel_structs.h
include/embt/linker/section_tags.h
include/embt/linker/sections.h
include/embt/logging/log.h
include/embt/logging/log_core.h
include/embt/logging/log_instance.h
include/embt/logging/log_msg.h
include/embt/sleep.h
include/embt/spinlock.h
include/embt/sys/cbprintf.h
include/embt/sys/cbprintf_cxx.h
include/embt/sys/cbprintf_enums.h
include/embt/sys/cbprintf_internal.h
include/embt/sys/clock.h
include/embt/sys/kobject.h
include/embt/sys/mem_stats.h
include/embt/sys/mpsc_packet.h
include/embt/sys/rb.h
include/embt/sys/sflist.h
include/embt/sys/sys_heap.h
include/embt/syscall.h
include/embt/tracing/tracing.h
include/embt/tracing/tracing_hooks.h
include/embt/tracing/tracing_macros.h
```

删除的 provider include 同步从消费者移除。需要真实 queue/work/timeout 声明的内部代码引用 base，
公开协议对象引用 storage；没有把内部 OSI/atomic 操作放进公开头。
没有批量删除协议动作、同步操作、回调或 source 文件来消除依赖。

## 5. 相关依赖整理

- k_sem 的已有存储定义从私有 base 头移到 storage.h，定义内容不变，供 RFCOMM/AVDTP 等
  公开协议对象内嵌字段使用；操作函数仍留在私有 base，不带出 OSI 原生类型。
- AVDTP 头补上标准整数头和实际 L2CAP 组件头，解决它原来依赖 include 顺序的问题。
- printk 保留当前真实输出声明，移除旧 CONFIG_LOG_PRINTK_STATIC 重定向分支。
- GNU toolchain 去掉已退休 native_sim/posix_trace 依赖，使用原有 __builtin_unreachable 路径。
- settings 仍按此前配置关闭，本轮移动并保留组件，不扩展为 persistence/runtime bring-up。
- 未启用的 Mesh/Audio/其他 profile 仍可能有待适配的 OS 调用或旧日志调用；本轮清理的是
  旧 include provider 和目录边界，不宣称所有可选协议组件都已可以编译/运行。

## 6. 验证

| 检查 | 结果 |
| --- | --- |
| 默认 mkbt.sh build | 基础 LE+BR 源码编译、静态库和 whole-archive consumer 链接通过 |
| mkbt.sh btshell | SYSTEM crypto 构建通过，16 项 CLI 行为检查通过 |
| SOURCE crypto btshell | 固定 Mbed TLS 3.6.5 构建/链接通过 |
| shell-enabled ASan/UBSan | 52/52 CTest 通过 |
| 独立公共头 | net_buf、Bluetooth/conn/gatt/l2cap、HCI driver、settings、random、RFCOMM、AVDTP 共 10 个头，C/C++ 各一次，共 20 项通过 |
| 移位/保留完整性 | 33 个文件的新位置和权限已核对，原非 Controller Bluetooth source 均有对应文件 |
| 废弃引用 | 实际 include 中不再引用旧 kernel/arch/device/syscall/logger/tracing 头；构建与测试代码没有旧 subsys/driver 路径 |

未重新占用 USB dongle；本轮没有新的无线操作或 Controller 运行结论。
既有初始化成功记录继续作为此前运行证据，当前变化以构建、CLI、回归和依赖检查验收。

原始校验清单和日志在 /tmp/embt-layout-cleanup：before-files.json、moves.json、header-audit.json、
content-verification.json、public-header-results.json、default-build.log、btshell-build.log、source-build.log、asan-tests.log。
本地 build 验证产物不进入待提交源码。
