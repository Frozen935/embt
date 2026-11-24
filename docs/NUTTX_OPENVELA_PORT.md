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

# OpenVela external 接入与原生 H4 验证

集成部署文件集中于 `integration/openvela/`，步骤见
[README](../integration/openvela/README.md)。

本版按 OpenVela 的 `build.sh` / `emulator.sh` 流程接入 ARM goldfish。
EmBT 库、btshell 注册、平台测试入口全部位于 `external/embt`；apps 仓库没有跟踪文件改动。
HCI 参考 zblue `port/drivers/bluetooth/hci/h4.c`，直接读写实际 H4 设备。
此前 apps 注册及 NuttX Bluetooth driver / user-channel 映射已撤掉。
当前仍是多线程和同步 HCI，交付保存在 task/nuttx-openvela-port 分支，以 Draft PR 提交，等待人工 review 和合入。

## 布局和构建边界

| 路径 / 所属仓库 | 内容 |
| --- | --- |
| EmBT `drivers/bluetooth/hci/h4.c` | H4 read/write/poll、分帧、buffer 转交、停止与错误处理；复用 OSI |
| EmBT `drivers/bluetooth/hci/h4/CMakeLists.txt` | 独立 H4 component，保留 Linux socket component |
| EmBT `include/embt/bluetooth/transport.h` | 增加 H4 endpoint 配置，不改原 socket API 名称 |
| EmBT `apps/btshell/runner.c` | 可用 transport 的 CLI，H4 默认路径来自同一 Kconfig |
| external `embt/CMakeLists.txt`, `Kconfig` | 库与 btshell 的两个独立 target，以及 OSI/settings/work 测试注册 |
| external `embt/source` | 指向唯一 EmBT task worktree；不复制 Host 源码 |
| external `embt/run_goldfish.sh` | 调用平台 emulator.sh，建立与当前驱动匹配的两个 MMIO serial device |
| external 根 `CMakeLists.txt` | embt profile 排除 zblue 配置树，其余 profile 排除 EmBT 树 |
| EmBT `integration/openvela/external_menu.cmake` | 在 external 接入层生成 source 清单，保留平台命名和递归菜单；NuttX 原生成器不修改 |
| vendor/openvela `boards/vela/configs/embt/defconfig` | 从 goldfish ARM preset 派生并由平台工具保存的真实配置 |
| vendor/openvela `boards/build/vendorsetup.sh` | lunch entry 和构建前的已有 Mbed TLS / cmocka 源检查 |

btshell 直接引用唯一 source 下的 main.c 和 runner.c。external 中调用
`nuttx_add_application(NAME btshell)`，平台生成入口 `btshell_main`。
主库仍是 `libembt.a`，组件获得平台真实 include/definitions，再进入 firmware 链接。
没有在 apps 下另建 app，也没有修改 Zephyr/NuttX Bluetooth Host 实现。

本版使用平台支持的 `build.sh --cmake` 模式。legacy Make 尚未交付。
上一版放在 Make.defs 的全局报错守卫会影响 CMake 中调用的 Make dependency context，
已经撤掉；没有空 Makefile、成功桩或全局 EmBT include 注入。

## 同一份配置

`Kconfig.embt` 是根 Kconfig 中原树的分离文件；没有第二套配置符号 namespace。
Kconfig 即使在 `if n` 下也解析定义，因此完整 zblue 与 EmBT 不能同时 source。
当前由 embt profile 选择 EmBT 树，原 zblue 源码与集成文件保留。

```text
OpenVela Kconfig + generated menus + EmBT Kconfig.embt
  -> platform setconfig / savedefconfig
  -> <output>/.config
  -> existing configure.py export
  -> generated/include/embt/autoconf.h + typed CMake values
  -> target-scoped include for EmBT and consumers
```

external root 接入辅助函数，vendor setup source 本目录 hook，均按 README 部署；
不会把宿主原文件或本地 NuttX 修改作为 EmBT 提交。

export 核对原生配置已求值，不创建子目录第二份 .config；平台目录拒绝 standalone 配置。
编译器来自 OpenVela 工具链，使用真实 `__NuttX__`、32 位和 little-endian 检查。
Linux/NuttX 继续共用 osdep/posix。Linux 使用默认 pthread attributes；NuttX 应用 stack
请求，并按默认 150 minus original priority 转换，未更改这套机制。
本 profile 请求 `SYSTEM_WORKQUEUE_STACK_SIZE=8192` 与
`EMBT_TIMEOUT_THREAD_STACK_SIZE=8192`，源码原默认值保持原样。

## H4 与虚拟设备

zblue port 的 h4.c 使用设备名和 open/read/write/poll，未经过 bt_driver_register。
goldfish 的 virtio-serial.c 通过 uart_register 暴露设备，一台 virtio serial device
对应一个 device index。配置 `ttyV0;ttyHCI0` 因而需要两个设备实例：
MMIO bus 3 的普通 console device 和 bus 7 的 HCI device。
一个设备内部的两个 port 不能生成这两个 NuttX device node。

本版 H4 直接打开 `/dev/ttyHCI0`，使用 O_BINARY、非阻塞读写和现有 OSI 生命周期。
Kernel `DRIVERS_BLUETOOTH=n`，EmBT `HCI_SOCKET=n`、`HCI_H4=y`；不使用 Linux HCI user ABI。
H4 仅覆盖当前基础 event/ACL 数据链路，SCO payload / ISO transport 未验收。

当前预置 emulator 的自动 BluetoothEmulation 添加 virtserialport 时缺少对应父总线；
virtio-console 又产生 PCI 设备，当前固件使用 MMIO。
显式 MMIO + Rootcanal chardev 可以启动并创建 ttyHCI0，但 bt init 触发 QEMU
`tcg_handle_interrupt: qemu_mutex_iothread_locked()` 断言，原始失败保留。

为继续验证实际 native Host，run_goldfish.sh 使用 QEMU 标准 socket chardev 对接既有
Bumble Controller。EmBT 路径始终是 H4 device read/write；TCP 位于模拟器虚拟硬件后端。
这不是 Rootcanal 通过证明，也不表示真实 Controller 无线验收完成。
没有增加 Host 锁屏障、暂停执行 wrapper 或 Controller 成功桩。

## 平台构建命令

任务平台根目录：`/tmp/embt-agents/openvela-nuttx-port`。
本版重新使用未修改的 NuttX 生成器构建，选树由 EmBT helper 完成。
源码与版本见 [OPENVELA_BASELINE](upstream/OPENVELA_BASELINE.md)。

```sh
CCACHE_DIR=/tmp/embt-agents/nuttx-openvela-evidence/ccache \
CCACHE_TEMPDIR=/tmp/embt-agents/nuttx-openvela-evidence/ccache/tmp \
./build.sh vendor/openvela/boards/vela/configs/embt --cmake \
  -b /tmp/embt-agents/nuttx-openvela-evidence/goldfish-embt-v3 -j4
```

配置变更走平台 setconfig，设置 srctree/BINDIR/KCONFIG_CONFIG/APPSDIR/APPSBINDIR，
以及 EXTERNALDIR/DRIVERS_PLATFORM_DIR；随后用同一 wrapper 的 savedefconfig 保存。
string 参数给工具传原始字符串，不再额外包一层 Kconfig 引号。

runtime helper 的存档位置为 integration/openvela，部署到 external/embt 后执行。

runtime helper 需要已有 Controller TCP server 的端口。先使用仓库现有
`test/bluetooth/classic/bumble/common/controllers.py tcp-server:127.0.0.1:0`，
读取其 ready line 中的端口，再启动 helper。工具不会下载或伪造 Controller：

```sh
external/embt/run_goldfish.sh <output> <controller-port> 5710
```

原生 NSH 中执行 btshell，默认 H4 路径为 ttyHCI0；bt init sync 显式开启蓝牙。
每个 boot 仅启动一次完整 btshell，退出前显式 disable；同进程重新启动 app 未验收。

## 证据和当前边界

证据目录 `/tmp/embt-agents/nuttx-openvela-evidence/`，失败不会被后续成功覆盖。
当前构建为 ARMv7-A，GCC 13.4.0；平台预置 emulator / QEMU 32.1.15.0。
所有原生结果来自完整 NuttX firmware，未手工定义 __NuttX__ 在主机模拟。

已执行：

- OpenVela wrapper 的完整 ARM 编译与 firmware 链接成功，包含 btshell_main 和 H4 component。
- 原生 NSH 启动、external app 注册、H4 ttyHCI0 device 出现；原生 PSA probe 通过。
- Linux OSI 24/24；Linux 默认 C 60 单元、1085 case 通过、19 原有 skip、0 failure。
- Linux 实际 PTY 上同一 H4 驱动处理拆分 H4 数据，完成 8 个 init/adv/scan/BR/disable 命令。
- 原生 H4 完整输出 Settings Loaded / Bluetooth initialized，完成广播、扫描、BR pscan/iscan、
  inquiry、disable、quit；单一 Controller 场景未验证 peer 连接。
- 原生 OSI 24 个 case 实际执行，23 通过、1 timer 上限断言失败、0 skip。
- 原生 work semantics 在独立 boot 中实际 8/8，通过保留原 condition-provider guard wrappers。
- 原生 FATFS settings save/load/delete 实测；下一个 boot load 成功，删除后 load 返回 -ENOENT。
- native 程序、VM 和原有 Controller model 的边界分别保留，结果汇总见 FINAL_NATIVE_RESULTS.json。

原始阻塞项：

- Rootcanal MMIO 收发触发预置 QEMU 内部断言；记录在 goldfish-final-runtime.log。
- 初始相对 settings 路径加载返回 -ENOENT；改为 /data/embt.settings 后，真实 FATFS 读写及跨 boot 恢复通过。
- 初始 timeout 1024-byte 请求对应实际栈 936 bytes，退出时 data abort 且填充 100%；
  profile 显式请求 timeout/system workqueue 8192 bytes。修改后的简单 disable/quit 没有该 data abort，
  不据此宣布全部线程退出/恢复生命周期验收完成。
- btshell 退出后同一 boot 再运行 work app 阻塞，涉及共享 static base runtime；
  原始序列保留在 final-native-runtime.log。base suite 只在 fresh boot 验证，不把隔离后的成功改写原失败。
- 原生 test_timer_rearm_stop 的 remaining <= requested 断言仍失败；原断言未修改。
- 平台保留 coredump device 缺失、mount failed 提示，以及 RWX LOAD segment 告警。
- 初始完整 preset 自动补了 SpeexDSP-1.2.1；该过程日志保留，下载源不进入交付。
  本版保留平台已有依赖配置，FFmpeg/media server 的 out-of-tree context 尚未解决，
  首期 profile 明确关闭这两个可选项；不是整个默认 multimedia preset 通过。

实机无线、Rootcanal、所有配置变体、RFCOMM 失败重试、ISO/audio、完整 security/GATT、
同一 NuttX 进程再次启动 btshell 均不在当前通过范围。
此前 x86 sim + TCP 的第一版统计只能作为历史比较，不替代本版 ARM/H4 验证。

## 打包提交验证

2026-11-24 使用 integration/openvela 部署文件、原版 NuttX Kconfig generator 重新验证。
ARM goldfish native .config 与之前实测配置逐字节相同，完整编译和链接成功；普通 sim:nsh
配置也通过平台 wrapper 构建。Linux test-port 本次执行 202/202 CTest，通过。

打包后的原生运行再次完成 FATFS save/load、PSA probe、H4 bt init sync、BLE 广播/扫描
start/stop、BR pscan/iscan、disable/quit。Controller 为既有 Bumble，QEMU 为普通 socket
chardev。日志保存在 pr-native-runtime.log；原版 NuttX 构建记录在 pr-native-final.log。
没有提交 nuttx/cmake/nuttx_kconfig.cmake 或它的 patch，也没有新增 skip。
这轮重建曾遇到 calculator asset 编译异常；源文件未修改，使用独立缓存并关闭缓存的
真实编译后通过。原因未单独闭环，失败日志保留，未修改无关源码。
